# nvcodec-shim

LD_PRELOAD workaround for an NVIDIA driver regression (>= 570) on multi-GPU
nodes: in a container that exposes only some of the node's GPUs,
libnvcuvid/libnvidia-encode query the Resource Manager for the full GPU list,
pick the lowest-UUID GPU as "primary", and fail with CUDA_ERROR_NO_DEVICE /
NV_ENC_ERR_UNSUPPORTED_DEVICE when that GPU's device node is absent.

The shim wraps `ioctl` and filters the attached/probed GPU id lists returned
by the driver down to GPUs whose `/dev/nvidiaN` exists in the container.
Everything else passes through untouched.

References:
- https://forums.developer.nvidia.com/t/347361
- https://github.com/NVIDIA/nvidia-container-toolkit/issues/1249

## Layout

- `shim/` — the C library.
- `Dockerfile`, `install.sh` — a busybox image whose entrypoint copies the
  library into a mounted directory; used as an init container.
- `policy.yaml` — a Kubernetes `MutatingAdmissionPolicy` (+ binding) that injects
  the init container, an emptyDir, a read-only mount and
  `LD_PRELOAD=/nvcodec-shim/libnvcodec-shim.so` into every container that
  requests `nvidia.com/gpu`. No webhook, no certificates: it runs inside the
  API server. Requires Kubernetes >= 1.34 with the `MutatingAdmissionPolicy`
  feature gate and the `admissionregistration.k8s.io/v1beta1` API enabled
  (both on by default once the API reaches GA).

An existing `LD_PRELOAD` on a GPU container is replaced.

Set `NVCODEC_SHIM_DEBUG=1` on a GPU container to log which GPUs the shim keeps
or hides.

## Image

Published to `ghcr.io/<owner>/nvcodec-shim` by GitHub Actions on every push to
`main` (`latest`, `sha-<commit>`) and on `v*` tags (`<major>.<minor>.<patch>`,
`<major>.<minor>`). Pull requests build without pushing.
