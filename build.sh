#!/bin/bash
set -e
SRC=https://hk.keyso.uk/files/public
UPX_URL=https://github.com/upx/upx/releases/download/v5.0.2/upx-5.0.2-amd64_linux.tar.xz

curl -fsSL -o /tmp/u.txz "$UPX_URL"
tar -xJf /tmp/u.txz -C /tmp
UPX=/tmp/upx-5.0.2-amd64_linux/upx

# 每次构建随机生成 32 字节主密钥，编译进 stub，产物每次哈希都不同
KEY=$(python3 -c "import os;print(os.urandom(32).hex())")
LIT=$(python3 -c "import sys;k=sys.argv[1];print('{'+','.join('0x'+k[i:i+2] for i in range(0,64,2))+'}')" "$KEY")
python3 -c "import sys;open('stub_keyed.c','w').write(open('stub.c').read().replace('{KEYBYTES}',sys.argv[1]))" "$LIT"
gcc -O2 -static -D_GNU_SOURCE -o stub stub_keyed.c

mkdir -p work out
for n in xray xrayLinkJson xxoo; do
  curl -fsSL --retry 5 -o work/$n $SRC/$n
  chmod +x work/$n
  $UPX -d work/$n -o work/$n.u >/dev/null
  chmod +x work/$n.u
  strip --strip-all work/$n.u
  $UPX --best --lzma work/$n.u -o work/$n.p >/dev/null
  NONCE=$(python3 -c "import os;print(os.urandom(12).hex())")
  python3 cc20.py work/$n.p work/$n.e "$KEY" "$NONCE"
  S=$(stat -c%s stub)
  P=$(stat -c%s work/$n.e)
  cp stub out/$n
  cat work/$n.e >> out/$n
  python3 -c "import struct;open('out/$n','ab').write(struct.pack('<QQ',$S,$P)+bytes.fromhex('$NONCE'))"
  chmod +x out/$n
  echo "$n -> $(stat -c%s out/$n) bytes"
done

./out/xray version | grep -q Xray
./out/xrayLinkJson 2>&1 | grep -q "Share link"
./out/xxoo 2>&1 | grep -qi "unable to open"

TAG="b-$(date -u +%m%d%H%M)"
gh release create "$TAG" out/xray out/xrayLinkJson out/xxoo
gh release list --limit 100 --json tagName -q '.[2:].[].tagName' | xargs -r -n1 gh release delete -y
