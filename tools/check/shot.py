import base64, asyncio, sys
from playwright.async_api import async_playwright
import os
ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),'..','..'))
TMP=sys.argv[1]; OUT=sys.argv[2]
sg=base64.b64encode(open(ROOT+'/tools/fonts/SpaceGrotesk.ttf','rb').read()).decode()
jb=base64.b64encode(open(ROOT+'/tools/fonts/JetBrainsMono.ttf','rb').read()).decode()
css=f"@font-face{{font-family:'Space Grotesk';src:url(data:font/ttf;base64,{sg});font-weight:300 700}}@font-face{{font-family:'JetBrains Mono';src:url(data:font/ttf;base64,{jb});font-weight:100 800}}"
async def main():
    async with async_playwright() as p:
        b=await p.chromium.launch(executable_path=os.environ.get('CHROMIUM') or None)  # CHROMIUM=/path to use a preinstalled browser
        pg=await b.new_page(viewport={'width':390,'height':844},device_scale_factor=2)
        async def fonts(route): await route.fulfill(status=200,content_type='text/css',body=css)
        await pg.route('**/fonts.googleapis.com/**',fonts)
        await pg.route('**/fonts.gstatic.com/**',lambda r: r.abort())
        await pg.goto('file://'+TMP+'/dash.html'); await pg.wait_for_timeout(400)
        await pg.screenshot(path=TMP+'/web_today.png',full_page=True)
        for t,n in [('t-qr','web_qr'),('t-gmail','web_gmail'),('t-cx','web_connect'),('t-sys','web_sys')]:
            await pg.click(f'[data-t={t}]'); await pg.wait_for_timeout(150)
            await pg.screenshot(path=TMP+'/'+n+'.png',full_page=True)
        await pg.goto('file://'+TMP+'/portal.html'); await pg.wait_for_timeout(300)
        await pg.screenshot(path=TMP+'/web_portal.png',full_page=True)
        await b.close()
asyncio.run(main())

# Contact sheet for the README: Today, Connect, setup mode (first ~1400 px of each)
from PIL import Image
panels=[Image.open(TMP+'/'+n+'.png') for n in ('web_today','web_connect','web_portal')]
H=1415
sheet=Image.new('RGB',(390*3+20,H),(13,31,51))
for i,im in enumerate(panels):
    im=im.resize((390,int(im.height*390/im.width)))
    sheet.paste(im.crop((0,0,390,min(H,im.height))),(i*400,0))
sheet.save(OUT+'/dashboard-previews.png')
