# %%
import cv2
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path


target_rgbs = np.array([
    (110, 50, 19),
    (104, 68, 55),
])

target_bgrs = target_rgbs[:, ::-1]

# %%
src_img_p = Path('pvz2.png')

img = cv2.imread(str(src_img_p))
mask = (img == target_bgrs[0]).all(axis=2) | (img == target_bgrs[1]).all(axis=2)

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
mask = (img == target_bgrs[0]).all(axis=2) | (img == target_bgrs[1]).all(axis=2)
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
peaks = peaks[i-1:]
plt.plot(peaks, col_sum[peaks], "x")

# %%
card_width = delta.mean()
cards = []

for peak1, peak2 in zip(peaks[:-1], peaks[1:]):
    cards.append(img[8+28:78+28, peak1:peak2])
    plt.imshow(cards[-1])
    plt.title(cards[-1].std())
    plt.show()
    

# %%
target_top_rgb = np.array((81, 33, 11))
plt.imshow((cards[0] == target_top_rgb[::-1]).all(axis=2)[:100, :])
# %%
# enable_card = cards[1]
# disable_card = cards[2]
# # plt.imshow(card)
# plt.subplot(1, 2, 1)
# plt.hist(enable_card.ravel(), bins=256, color='r', alpha=0.5)
# plt.subplot(1, 2, 2)
# plt.hist(disable_card.ravel(), bins=256, color='b', alpha=0.5)
# %%
fig, axes = plt.subplots(3, 4, figsize=(12, 9))
axes = axes.ravel()

for ax, card in zip(axes, cards):
    ax.hist(card.ravel(), bins=128, color='r', alpha=0.5)
    less_50 = (card < 50).sum()
    more_200 = (card > 200).sum()
    ax.set_title(f"{more_200:3d} / {less_50:3d} = {more_200 / less_50:.2f}")

plt.show()
fix, axes = plt.subplots(3, 4, figsize=(12, 9))
axes = axes.ravel()

for ax, card in zip(axes, cards):
    ax.imshow(card)
plt.show()

# %%
