// nvcodec-shim: LD_PRELOAD workaround for NVIDIA driver >= 570 in containers
// that expose a subset of a multi-GPU node's cards. libnvcuvid/libnvidia-encode
// ask the Resource Manager for the attached/probed GPU id lists, get every GPU
// on the host, pick the lowest-UUID one as "primary", and fail when its device
// node isn't present. This filters those lists down to GPUs whose /dev/nvidiaN
// exists in the container. Struct layouts from open-gpu-kernel-modules.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#define NV_IOCTL_MAGIC 'F'
#define NV_ESC_RM_CONTROL 0x2A
#define NV0000_CTRL_CMD_GPU_GET_ATTACHED_IDS 0x201
#define NV0000_CTRL_CMD_GPU_GET_PROBED_IDS 0x214
#define NV0000_CTRL_GPU_MAX_IDS 32
#define NV0000_CTRL_GPU_INVALID_ID 0xffffffffu

typedef struct {
  uint32_t hClient, hObject, cmd, flags;
  uint64_t params;
  uint32_t paramsSize, status;
} NVOS54_PARAMETERS;

static int (*real_ioctl)(int, unsigned long, ...);
static int debug;

static int is_nvidiactl(int fd) {
  char path[64], target[256];
  snprintf(path, sizeof path, "/proc/self/fd/%d", fd);
  ssize_t n = readlink(path, target, sizeof target - 1);
  if (n < 0) return 0;
  target[n] = 0;
  return strcmp(target, "/dev/nvidiactl") == 0;
}

// gpuId encodes PCI domain:bus:device; resolve to the device minor via procfs
// and report whether that node is visible here. Unknown => keep (fail open).
static int gpu_visible(uint32_t id) {
  char path[128], line[256];
  snprintf(path, sizeof path, "/proc/driver/nvidia/gpus/%04x:%02x:%02x.0/information",
           (id >> 16) & 0xffff, (id >> 8) & 0xff, id & 0xff);
  FILE *f = fopen(path, "r");
  if (!f) return 1;
  int minor = -1;
  while (fgets(line, sizeof line, f))
    if (sscanf(line, "Device Minor: %d", &minor) == 1) break;
  fclose(f);
  if (minor < 0) return 1;
  snprintf(path, sizeof path, "/dev/nvidia%d", minor);
  struct stat st;
  return stat(path, &st) == 0;
}

static void filter_ids(uint32_t *ids) {
  int out = 0;
  for (int i = 0; i < NV0000_CTRL_GPU_MAX_IDS && ids[i] != NV0000_CTRL_GPU_INVALID_ID; i++) {
    int vis = gpu_visible(ids[i]);
    if (debug) fprintf(stderr, "[nvcodec-shim] gpu 0x%x %s\n", ids[i], vis ? "kept" : "hidden");
    if (vis) ids[out++] = ids[i];
  }
  for (; out < NV0000_CTRL_GPU_MAX_IDS; out++) ids[out] = NV0000_CTRL_GPU_INVALID_ID;
}

int ioctl(int fd, unsigned long req, ...) {
  va_list ap; va_start(ap, req); void *arg = va_arg(ap, void *); va_end(ap);
  if (!real_ioctl) { real_ioctl = dlsym(RTLD_NEXT, "ioctl"); debug = getenv("NVCODEC_SHIM_DEBUG") != NULL; }
  int ret = real_ioctl(fd, req, arg);
  if (ret == 0 && _IOC_TYPE(req) == NV_IOCTL_MAGIC && _IOC_NR(req) == NV_ESC_RM_CONTROL && is_nvidiactl(fd)) {
    NVOS54_PARAMETERS *p = arg;
    if (p->status == 0 && (p->cmd == NV0000_CTRL_CMD_GPU_GET_ATTACHED_IDS || p->cmd == NV0000_CTRL_CMD_GPU_GET_PROBED_IDS)
        && p->paramsSize >= NV0000_CTRL_GPU_MAX_IDS * sizeof(uint32_t))
      filter_ids((uint32_t *)(uintptr_t)p->params);
  }
  return ret;
}
