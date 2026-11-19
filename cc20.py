import sys, os, struct
def rotl(v,c): return ((v<<c)|(v>>(32-c))) & 0xffffffff
def qr(x,a,b,c,d):
    x[a]=(x[a]+x[b])&0xffffffff; x[d]^=x[a]; x[d]=rotl(x[d],16)
    x[c]=(x[c]+x[d])&0xffffffff; x[b]^=x[c]; x[b]=rotl(x[b],12)
    x[a]=(x[a]+x[b])&0xffffffff; x[d]^=x[a]; x[d]=rotl(x[d],8)
    x[c]=(x[c]+x[d])&0xffffffff; x[b]^=x[c]; x[b]=rotl(x[b],7)
def block(key,nonce,ctr):
    st=[0x61707865,0x3320646e,0x79622d32,0x6b206574]
    st+= list(struct.unpack("<8I", key))
    st+= [ctr]+list(struct.unpack("<3I", nonce))
    x=st[:]
    for _ in range(10):
        qr(x,0,4,8,12); qr(x,1,5,9,13); qr(x,2,6,10,14); qr(x,3,7,11,15)
        qr(x,0,5,10,15); qr(x,1,6,11,12); qr(x,2,7,8,13); qr(x,3,4,9,14)
    return struct.pack("<16I", *[(x[i]+st[i])&0xffffffff for i in range(16)])
def crypt(data,key,nonce):
    out=bytearray(data); ctr=1; off=0
    while off < len(data):
        ks=block(key,nonce,ctr); ctr+=1
        for i in range(min(64,len(data)-off)):
            out[off+i]^=ks[i]
        off+=64
    return bytes(out)
if __name__=="__main__":
    data=open(sys.argv[1],"rb").read()
    key=bytes.fromhex(sys.argv[3]); nonce=bytes.fromhex(sys.argv[4])
    open(sys.argv[2],"wb").write(crypt(data,key,nonce))
