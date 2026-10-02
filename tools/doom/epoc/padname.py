import re,sys
def pad(n):
    m=re.match(r'^([A-Za-z0-9_]+)\[([0-9a-fA-F]+)\](\.[A-Za-z]+)$',n)
    b,h,e=m.groups()
    h=h.lstrip('0') or '0'
    while len(h)<8: h='0'+h
    while (len(b)+2+len(h)+len(e)+1)%4!=0: h='0'+h
    return f'{b}[{h}]{e}'
if __name__=='__main__':
    for a in sys.argv[1:]: print(pad(a))
