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
        b=await p.chromium.launch()
        pg=await b.new_page(viewport={'width':390,'height':844},device_scale_factor=2)
        async def fonts(route): await route.fulfill(status=200,content_type='text/css',body=css)
        await pg.route('**/fonts.googleapis.com/**',fonts)
        await pg.route('**/fonts.gstatic.com/**',lambda r: r.abort())
        await pg.goto('file://'+TMP+'/dash.html'); await pg.wait_for_timeout(400)
        await pg.screenshot(path=OUT+'/web_today.png',full_page=True)
        for t,n in [('t-qr','web_qr'),('t-gmail','web_gmail'),('t-sys','web_sys')]:
            await pg.click(f'[data-t={t}]'); await pg.wait_for_timeout(150)
            await pg.screenshot(path=OUT+'/'+n+'.png',full_page=True)
        await pg.goto('file://'+TMP+'/portal.html'); await pg.wait_for_timeout(300)
        await pg.screenshot(path=OUT+'/web_portal.png',full_page=False)
        await b.close()
asyncio.run(main())
