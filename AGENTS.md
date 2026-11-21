# Repository Guidelines

## 仓库用途

- `android_kernel_samsung_sm7325/`：Samsung SM7325（`gts7fewifi`）Android 5.4 内核源码。
- `android_kernel_samsung_sm7325/.github/workflows/build-sm7325.yml`：GitHub Actions 工作流。构建、配置校验和产物发布相关改动放在这里。

## 工作流程

1. 在 `android_kernel_samsung_sm7325/` 中从当前基线创建功能分支，例如 `feat/<简短名称>`，然后修改内核代码。
2. 完成功能后，按“提交前检查”执行本地检查，提交简洁的中文 commit，并将分支推送到远端。
3. 在 GitHub Actions 手动运行 `build-sm7325` 云端工作流，并将 `kernel_ref` 设置为待验证的功能分支、标签或提交 SHA。工作流使用 `vendor/gts7fewifi_eur_open_defconfig` 构建 ARM64 内核，并上传内核镜像、AnyKernel3 ZIP、校验和及配置文件。
4. 构建成功后，拉取产物推送到真机下载目录，提醒用户进行实机测试，并等待测试反馈。
5. 根据实机反馈处理后续工作：测试失败时根据用户真机日志继续在功能分支修复，重新检查、提交、推送并构建；测试通过时，提醒用户将功能分支的修改迁移到主线分支。
6. 构建失败时，下载工作流产出的构建日志，定位失败原因；修复代码后重新检查、提交并推送，再次运行云端构建，直到构建成功。

`build-sm7325.yml` 仅用于维护上述 GitHub Actions。请一定要使用云端构建！
