import qrcode, sys
cap=[0,17,32,53,78,106,134,154,192,230,271,321,367,425,458,520,586]
texts=sys.argv[2:]
out=['// generated','struct SimQR{const char* t; int size; const char* bits;};','static const SimQR SIM_QR[]={']
for t in texts:
    v=next(v for v in range(1,17) if len(t.encode())<=cap[v])
    q=qrcode.QRCode(version=v,error_correction=qrcode.constants.ERROR_CORRECT_L,border=0,box_size=1)
    q.add_data(t.encode(), optimize=0); q.make(fit=False)
    m=q.get_matrix()
    bits=''.join('1' if c else '0' for row in m for c in row)
    out.append('{%s,%d,"%s"},'%(json:=__import__('json').dumps(t),len(m),bits))
out.append('};')
open(sys.argv[1],'w').write('\n'.join(out))
