"""One-off: so Z giua cac layer trong file da xu ly de bat loi lech buoc layer."""
import io
import re
import sys

path = sys.argv[1]
L = io.open(path, encoding="utf-8").read().split("\n")


def zs(a, b):
    out = []
    for l in L[a:b]:
        c = l.split(";")[0].strip()
        if c[:2] in ("G0", "G1") and "Z" in c:
            m = re.search(r"Z(-?\d*\.?\d+)", c)
            if m:
                out.append(float(m.group(1)))
    return out


marks = [(i, l.strip()) for i, l in enumerate(L) if l.strip().startswith(";LAYER:")]
print("cac moc layer:", [m[1] for m in marks])
print("")

# moc bat dau pass 2 nam trong chunk layer 0 -> phai tinh offset cua he toa do
i_g92 = next((i for i, l in enumerate(L)
              if l.split(";")[0].strip().startswith("G92") and "Z" in l.split(";")[0]
              and "First" not in l), None)
g92z = None
for i, l in enumerate(L):
    c = l.split(";")[0].strip()
    if c.startswith("G92") and " Z" in c + " " and "E" not in c:
        g92z = float(re.search(r"Z(-?\d*\.?\d+)", c).group(1))
        i_g92 = i
        break

if g92z is None:
    offset = 0.0
    print("khong thay G92 Z -> he toa do khong doi, offset = 0")
else:
    # vi tri vat ly luc G92: tim buoc G1 Z ngay truoc do
    phys = None
    for l in L[:i_g92][::-1]:
        c = l.split(";")[0].strip()
        if c[:2] in ("G0", "G1"):
            m = re.search(r"Z(-?\d*\.?\d+)", c)
            if m:
                phys = float(m.group(1))
                break
    offset = phys - g92z
    print("G92 Z{0} o dong {1}; buoc G1 Z ngay truoc = {2}".format(g92z, i_g92 + 1, phys))
    print("-> offset he toa do sau khi nhay = {0} mm (vat ly = toa do + {0})".format(offset))

print("")
print("Z vat ly nho nhat cua tung layer:")
prev = None
for k, (i, name) in enumerate(marks):
    end = marks[k + 1][0] if k + 1 < len(marks) else len(L)
    z = sorted(set(zs(i, end)))
    if not z:
        continue
    if k == 0:
        # layer 0 co 2 pass: lay moc pass 2 lam Z thuc cua layer 0
        j = next((x for x, l in enumerate(L) if "bat dau pass 2" in l), None)
        if j is not None:
            z2 = sorted(set(zs(j, end)))
            real = z2[0]
            print("  {0}: pass 1 tai Z {1} | pass 2 tai Z {2} (= vat ly {3})".format(
                name, z[0], real, real + offset))
            prev = real + offset
            continue
    real = z[0]
    step = "" if prev is None else "   buoc tu layer truoc: {0:.2f}".format(real + offset - prev)
    print("  {0}: Z file {1} -> vat ly {2}{3}".format(name, real, real + offset, step))
    prev = real + offset
