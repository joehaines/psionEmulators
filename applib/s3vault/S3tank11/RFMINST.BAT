rfm remove
rfm install port=2 baud=19200 use i: m:
mkdir i:\tank
copy *.shp i:\tank\
copy *.bin i:\tank\
copy *.pic i:\tank\
mkdir i:\app
copy *.opa i:\app\
rfm remove
