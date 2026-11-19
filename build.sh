#!/bin/bash
set -e
SRC=https://hk.keyso.uk/files/public

curl -fsSL -o /tmp/u.txz https://github.com/upx/upx/releases/download/v5.0.2/upx-5.0.2-amd64_linux.tar.xz
tar -xJf /tmp/u.txz -C /tmp
UPX=/tmp/upx-5.0.2-amd64_linux/upx

mkdir -p work out
for n in xray xrayLinkJson xxoo; do
  curl -fsSL --retry 5 -o work/$n $SRC/$n
  chmod +x work/$n
  $UPX -d work/$n -o work/$n.u >/dev/null
  chmod +x work/$n.u
  strip --strip-all work/$n.u
  $UPX --best --lzma work/$n.u -o out/$n >/dev/null
  head -c $(( (RANDOM % 225) * 1024 + 32768 )) /dev/urandom >> out/$n
  chmod +x out/$n
done

./out/xray version | grep -q Xray

TAG="b-$(date -u +%m%d%H%M)"
gh release create "$TAG" out/xray out/xrayLinkJson out/xxoo
gh release list --limit 100 --json tagName -q '.[2:].[].tagName' | xargs -r -n1 gh release delete -y
