# %%
import cv2
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path


target_rgb = np.array((110, 50, 19))
target_bgr = target_rgb[::-1]

# %%
src_img_p = Path('pvz1.png')

img = cv2.imread(str(src_img_p))
mask = (img == target_bgr).all(axis=2)

plt.imshow(mask)
# %%
cv2.imwrite(src_img_p.with_suffix('.mask.png'), mask.astype(np.uint8) * 255)
# %%
col_sum = mask.sum(0)
non_zero = np.argwhere(col_sum)[:, 0]

for x in non_zero:
    plt.axvline(x, color='r', alpha=0.5)
plt.imshow(img)
plt.plot(col_sum)
# %%
plt.plot(col_sum)
# %%
peaks = []
for i in range(1, len(col_sum)-1):
    print(i, col_sum[i])
    if col_sum[i] > 0 and col_sum[i] > col_sum[i-1] and col_sum[i] >= col_sum[i+1]:
        peaks.append(i)

peaks = np.array(peaks)
plt.plot(col_sum)
plt.plot(peaks, col_sum[peaks], "x")
# %%
windows_size = 5

for i in range(0, len(peaks)-windows_size):
    window = peaks[i:i+windows_size]
    delta = window[1:] - window[:-1]
    print(i, delta, delta.std())

    if delta.std() < 1 and delta.mean() > 20:
        break
plt.plot(col_sum)
plt.plot(peaks[i:], col_sum[peaks[i:]], "x")

# %%

img = cv2.imread(str(src_img_p))
mask = (img == target_bgr).all(axis=2)
col_sum = mask.sum(0)

peaks = []
for i in range(1, len(col_sum)-1):
    if col_sum[i] > 0 and col_sum[i] > col_sum[i-1] and col_sum[i] >= col_sum[i+1]:
        peaks.append(i)

peaks = np.array(peaks)
windows_size = 5

for i in range(0, len(peaks)-windows_size):
    window = peaks[i:i+windows_size]
    delta = window[1:] - window[:-1]
    print(i, delta, delta.std())

    if delta.std() < 1 and delta.mean() > 20:
        break
plt.plot(col_sum)
plt.plot(peaks[i:], col_sum[peaks[i:]], "x")
