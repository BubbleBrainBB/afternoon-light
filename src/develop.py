#!/usr/bin/env python3
"""develop.py — "darkroom" for the path tracer output.

Reads the linear float AOVs written by ./render and produces the final image:
feature-guided denoise of the indirect light, light shafts, bloom, filmic tone
curve, colour grade, vignette, fine grain, dithered 8-bit sRGB.

usage: develop.py PREFIX OUT.png [--preview OUT.jpg] [--fast]
"""
import sys, argparse
import numpy as np
from PIL import Image
from scipy import ndimage as ndi


def load(prefix):
    w, h, spp, px0, py0, fw, fh = [int(v) for v in open(prefix + ".txt").read().split()]
    def rd(name, c):
        a = np.fromfile(f"{prefix}_{name}.f32", dtype=np.float32)
        return a.reshape(h, w, c) if c > 1 else a.reshape(h, w)
    motes = np.fromfile(f"{prefix}_motes.f32", dtype=np.float32).reshape(-1, 4)
    return dict(w=w, h=h, spp=spp, px0=px0, py0=py0, fw=fw, fh=fh, motes=motes, beauty=rd("beauty", 3), sun=rd("sun", 3), alb=rd("alb", 3),
                nrm=rd("nrm", 3), vol=rd("vol", 3), depth=rd("depth", 1))


def lum(c):
    return 0.2126 * c[..., 0] + 0.7152 * c[..., 1] + 0.0722 * c[..., 2]


def denoise_indirect(ind, alb, nrm, depth, iters, sigma_c=0.35):
    """Edge-avoiding a-trous wavelet filter on demodulated indirect light,
    guided by normal / depth / albedo so texture and geometry edges survive."""
    albc = np.maximum(alb, 0.02)
    x = ind / albc
    # firefly rejection: no pixel may exceed a multiple of its neighbourhood median
    med = np.stack([ndi.median_filter(x[..., c], size=3) for c in range(3)], axis=-1)
    import os
    ff = float(os.environ.get('FF', '3.5'))
    if ff > 0:
        x = np.minimum(x, ff * med + 0.02)
    k = np.array([1, 4, 6, 4, 1], dtype=np.float32) / 16.0
    ldepth = np.log(np.maximum(depth, 1e-3))
    nlen = np.linalg.norm(nrm, axis=-1, keepdims=True)
    nrm = nrm / np.maximum(nlen, 1e-4)
    for it in range(iters):
        step = 1 << it
        acc = np.zeros_like(x)
        wsum = np.zeros(x.shape[:2], dtype=np.float32)
        xl = lum(x)
        # local scale for the colour edge-stopping term
        sc = sigma_c * (ndi.uniform_filter(xl, 7) + 0.02)
        for dy in range(-2, 3):
            for dx in range(-2, 3):
                wk = k[dy + 2] * k[dx + 2]
                if dy == 0 and dx == 0:
                    acc += x * wk
                    wsum += wk
                    continue
                sh = (dy * step, dx * step)
                xs = np.roll(x, sh, axis=(0, 1))
                ns = np.roll(nrm, sh, axis=(0, 1))
                ds = np.roll(ldepth, sh, axis=(0, 1))
                as_ = np.roll(albc, sh, axis=(0, 1))
                wn = np.clip((ns * nrm).sum(-1), 0, 1) ** 24
                wd = np.exp(-np.abs(ds - ldepth) / 0.02)
                wa = np.exp(-np.abs(as_ - albc).sum(-1) / 0.25)
                wc = np.exp(-np.abs(lum(xs) - xl) / (sc * (2.0 ** -it) + 1e-4))
                wgt = (wk * wn * wd * wa * wc).astype(np.float32)
                # np.roll wraps around: discard taps that came from the opposite border
                sy, sx = sh
                if sy > 0: wgt[:sy, :] = 0
                elif sy < 0: wgt[sy:, :] = 0
                if sx > 0: wgt[:, :sx] = 0
                elif sx < 0: wgt[:, sx:] = 0
                acc += xs * wgt[..., None]
                wsum += wgt
        x = acc / wsum[..., None]
    return x * albc


def splat_motes(B, scale):
    """Dust motes: each is a small soft disc (defocus); energy is conserved so the sharp ones sparkle."""
    h, w = B["h"], B["w"]
    acc = np.zeros((h, w), dtype=np.float32)
    for x, y, coc, inten in B["motes"]:
        r = max(max(coc, 0.75 * scale) + 0.35 * scale, 0.6)
        R = int(np.ceil(2.5 * r + 1))
        xi, yi = int(round(x)), int(round(y))
        x0, x1, y0, y1 = max(xi - R, 0), min(xi + R + 1, w), max(yi - R, 0), min(yi + R + 1, h)
        if x0 >= x1 or y0 >= y1:
            continue
        yy, xx = np.mgrid[y0:y1, x0:x1]
        d2 = (xx - x) ** 2 + (yy - y) ** 2
        k = np.exp(-d2 / (r * r))
        acc[y0:y1, x0:x1] += k * (inten * scale * scale / max(k.sum(), 1e-6))
    return acc[..., None]


def gauss(img, s):
    return ndi.gaussian_filter(img, sigma=(s, s, 0) if img.ndim == 3 else s, mode="nearest")


def tonemap(x):
    """Filmic curve on a hue-preserving blend of per-channel and luminance mapping."""
    def curve(v):
        # soft toe, long gentle shoulder (rational filmic fit)
        a, b, c, d, e = 2.20, 0.06, 2.10, 0.62, 0.20
        return np.clip((v * (a * v + b)) / (v * (c * v + d) + e), 0, 1)
    pc = curve(x)
    L = lum(x)[..., None]
    lc = x * (curve(L) / np.maximum(L, 1e-6))
    out = 0.7 * pc + 0.3 * np.clip(lc, 0, 1)
    return out


def srgb(x):
    x = np.clip(x, 0, 1)
    return np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1 / 2.4) - 0.055)


def develop(prefix, out, preview=None, fast=False, exposure=1.0, shafts=1.0, full_w=None):
    B = load(prefix)
    w, h = B["w"], B["h"]
    scale = (full_w or w) / 3840.0  # pixel-size dependent radii are defined for the 3840 master
    beauty, sun = B["beauty"], B["sun"]
    ind = np.maximum(beauty - sun, 0)
    import os
    cache = prefix + "_dn.npz"   # denoised layers, so grading tweaks do not redo the filtering
    have = (not fast) and os.path.exists(cache) and os.path.getmtime(cache) > os.path.getmtime(prefix + "_beauty.f32")
    if have:
        z = np.load(cache); ind = z["ind"]; vol_dn = z["vol"]
    elif not fast:
        iters = 4 if B["spp"] >= 256 else 5
        ind = denoise_indirect(ind, B["alb"], B["nrm"], B["depth"], iters)
    img = sun + ind

    # light shafts: single-scattered sunlight in the room air (rendered per unit density)
    vol = B["vol"]
    if have:
        vol = vol_dn
    elif not fast:
        vol = denoise_indirect(vol, np.ones_like(vol), B["nrm"], B["depth"], 3, sigma_c=0.6)
        vol = gauss(vol, 0.8 * scale)
        np.savez(cache, ind=ind.astype(np.float32), vol=vol.astype(np.float32))
    img = img + vol * (0.14 * shafts)
    motes_k = float(os.environ.get('MOTES', '0.8'))
    img = img + splat_motes(B, scale) * np.array([1.0, 0.86, 0.66], dtype=np.float32) * (motes_k * shafts)

    # bloom / veiling glare: wide, faint
    hi = np.maximum(img - 1.2, 0)
    bloom = 0.55 * gauss(hi, 6 * scale) + 0.30 * gauss(hi, 24 * scale) + 0.15 * gauss(hi, 90 * scale)
    img = img + 0.10 * bloom

    img = img * (1.75 * exposure)
    # white balance: keep the sun warm, let shadows lean a touch cool
    img = img * np.array([1.0, 0.985, 0.95], dtype=np.float32)
    out_img = tonemap(img)

    # gentle grade: slight warmth in highlights, cool lift in shadows, soft S contrast
    L = lum(out_img)[..., None]
    out_img = out_img + (1 - L) ** 2.5 * np.array([-0.010, 0.000, 0.016]) + L ** 2 * np.array([0.012, 0.004, -0.010])
    sat = 1.06
    out_img = L + (out_img - L) * sat
    out_img = np.clip(out_img, 0, 1)

    # vignette
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    r2 = ((xx / w - 0.5) ** 2 + ((yy / h - 0.5) * h / w) ** 2)
    out_img = out_img * (1 - 0.30 * np.clip(r2 / 0.39, 0, 1) ** 1.3)[..., None]

    enc = srgb(out_img)
    rng = np.random.default_rng(7)
    # fine photographic grain (luminance), strongest in midtones; doubles as dither
    g = rng.normal(0, 1, (h, w)).astype(np.float32)
    g = 0.6 * g + 0.4 * ndi.gaussian_filter(g, 0.7) * 2.2
    Le = lum(enc)
    enc = enc + (g * 0.0075 * (0.35 + 1.3 * Le * (1 - Le) * 2))[..., None]
    enc = enc + (rng.random((h, w, 3)).astype(np.float32) - 0.5) / 255.0
    im8 = (np.clip(enc, 0, 1) * 255 + 0.5).astype(np.uint8)
    im = Image.fromarray(im8, "RGB")
    if out.lower().endswith(".png"):
        im.save(out, optimize=True)
    else:
        im.save(out, quality=93)
    if preview:
        pw = 1920
        im.resize((pw, round(h * pw / w)), Image.LANCZOS).save(preview, quality=92, subsampling=0)
    print("wrote", out, im.size)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("prefix"); ap.add_argument("out")
    ap.add_argument("--preview"); ap.add_argument("--fast", action="store_true")
    ap.add_argument("--exposure", type=float, default=1.0)
    ap.add_argument("--shafts", type=float, default=1.0)
    ap.add_argument("--full-w", type=int, default=None)
    a = ap.parse_args()
    develop(a.prefix, a.out, a.preview, a.fast, a.exposure, a.shafts, a.full_w)
