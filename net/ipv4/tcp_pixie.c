// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <net/tcp.h>

#define PIXIE_SAMPLES	256
#define PIXIE_MASK	(PIXIE_SAMPLES - 1)

static int rate = 100000000;
static int feedback = 2;

static int pixie_set_positive(const char *val, const struct kernel_param *kp)
{
	int value, ret = kstrtoint(val, 0, &value);

	if (ret)
		return ret;
	if (value <= 0 || (kp->arg == &feedback && value > 64))
		return -EINVAL;
	WRITE_ONCE(*(int *)kp->arg, value);
	return 0;
}

static const struct kernel_param_ops pixie_param_ops = {
	.set = pixie_set_positive,
	.get = param_get_int,
};
module_param_cb(rate, &pixie_param_ops, &rate, 0644);
MODULE_PARM_DESC(rate, "target pacing rate in bytes per second");
module_param_cb(feedback, &pixie_param_ops, &feedback, 0644);
MODULE_PARM_DESC(feedback, "RTT feedback window multiplier (1-64)");

struct pixie_sample {
	u32 acked;
	u32 losses;
	u32 stamp_us;
};

struct pixie {
	u64 rate;
	u64 curr_acked;
	u64 curr_losses;
	struct pixie_sample *samples;
	u16 start;
	u16 count;
};

static void pixie_remove_oldest(struct pixie *ca)
{
	struct pixie_sample *sample = &ca->samples[ca->start];

	ca->curr_acked -= sample->acked;
	ca->curr_losses -= sample->losses;
	ca->start = (ca->start + 1) & PIXIE_MASK;
	ca->count--;
}

static void pixie_record(struct pixie *ca, const struct rate_sample *rs,
			 u32 now, u32 window)
{
	struct pixie_sample *sample;
	u32 bucket = DIV_ROUND_UP(window, PIXIE_SAMPLES - 1);

	while (ca->count &&
	       (u32)(now - ca->samples[ca->start].stamp_us) > window)
		pixie_remove_oldest(ca);

	if (ca->count) {
		sample = &ca->samples[(ca->start + ca->count - 1) & PIXIE_MASK];
		if ((u32)(now - sample->stamp_us) < bucket &&
		    rs->acked_sacked <= U32_MAX - sample->acked &&
		    rs->losses <= U32_MAX - sample->losses)
			goto accumulate;
	}

	if (ca->count == PIXIE_SAMPLES)
		pixie_remove_oldest(ca);
	sample = &ca->samples[(ca->start + ca->count) & PIXIE_MASK];
	sample->acked = 0;
	sample->losses = 0;
	sample->stamp_us = now;
	ca->count++;

accumulate:
	sample->acked += rs->acked_sacked;
	sample->losses += rs->losses;
	ca->curr_acked += rs->acked_sacked;
	ca->curr_losses += rs->losses;
}

static void pixie_update(struct sock *sk, const struct rate_sample *rs)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct pixie *ca = inet_csk_ca(sk);
	u64 pacing = ca->rate, cwnd;
	u32 rtt = tp->srtt_us >> 3;

	if (rs->delivered < 0 || rs->interval_us <= 0 || rs->losses < 0)
		return;

	if (ca->samples) {
		u32 window = clamp_t(u64,
			(u64)READ_ONCE(feedback) * tp->srtt_us / 2,
			1, U32_MAX / 2);

		pixie_record(ca, rs, (u32)tp->tcp_mstamp, window);
		if (ca->curr_acked) {
			u64 total = ca->curr_acked + ca->curr_losses;
			unsigned int shift = max(fls64(total) - 32, 0);
			u64 acked = max_t(u64, ca->curr_acked >> shift, 1);

			pacing = div64_u64(ca->rate * (total >> shift), acked);
		}
	}

	WRITE_ONCE(sk->sk_pacing_rate,
		   min_t(u64, pacing, READ_ONCE(sk->sk_max_pacing_rate)));
	if (!rtt || !tp->mss_cache)
		return;

	cwnd = min_t(u64, div_u64(pacing, tp->mss_cache), U32_MAX);
	cwnd = div_u64(cwnd * rtt, USEC_PER_SEC);
	if (!ca->samples)
		cwnd *= 2;
	tp->snd_cwnd = min_t(u64, max_t(u64, cwnd, 2), tp->snd_cwnd_clamp);
}

static void pixie_init(struct sock *sk)
{
	struct pixie *ca = inet_csk_ca(sk);

	ca->rate = READ_ONCE(rate);
	ca->start = 0;
	ca->count = 0;
	ca->curr_acked = 0;
	ca->curr_losses = 0;
	ca->samples = kmalloc_array(PIXIE_SAMPLES, sizeof(*ca->samples),
				    GFP_ATOMIC | __GFP_NOWARN);
	cmpxchg(&sk->sk_pacing_status, SK_PACING_NONE, SK_PACING_NEEDED);
}

static void pixie_release(struct sock *sk)
{
	struct pixie *ca = inet_csk_ca(sk);

	kfree(ca->samples);
	ca->samples = NULL;
}

static u32 pixie_ssthresh(struct sock *sk)
{
	return TCP_INFINITE_SSTHRESH;
}

static u32 pixie_undo_cwnd(struct sock *sk)
{
	return tcp_sk(sk)->snd_cwnd;
}

static struct tcp_congestion_ops tcp_pixie __read_mostly = {
	.flags		= TCP_CONG_NON_RESTRICTED,
	.name		= "pixie",
	.owner		= THIS_MODULE,
	.init		= pixie_init,
	.release	= pixie_release,
	.cong_control	= pixie_update,
	.ssthresh	= pixie_ssthresh,
	.undo_cwnd	= pixie_undo_cwnd,
};

static int __init pixie_register(void)
{
	BUILD_BUG_ON(sizeof(struct pixie) > ICSK_CA_PRIV_SIZE);
	BUILD_BUG_ON(PIXIE_SAMPLES & PIXIE_MASK);
	BUILD_BUG_ON(PIXIE_SAMPLES * sizeof(struct pixie_sample) > PAGE_SIZE);
	return tcp_register_congestion_control(&tcp_pixie);
}

static void __exit pixie_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_pixie);
}

module_init(pixie_register);
module_exit(pixie_unregister);

MODULE_AUTHOR("marywangran");
MODULE_DESCRIPTION("Pixie TCP congestion control");
MODULE_LICENSE("GPL");
