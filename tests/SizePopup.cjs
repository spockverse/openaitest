const { chromium } = require(process.env.WUBROT_PLAYWRIGHT || 'playwright');
const fs = require('fs'), assert = require('assert'), path = require('path');
(async () => {
  const browser = await chromium.launch({
    ...(process.env.WUBROT_CHROMIUM ? { executablePath: process.env.WUBROT_CHROMIUM } : {}),
    args: ['--no-sandbox']
  });
  try {
    for (const deviceScaleFactor of [1, 1.25, 1.5, 1.75, 2]) {
      for (const [index, width, height] of [[0,600,520], [1,804,697], [2,996,864]]) {
        const page = await browser.newPage({ viewport: { width, height }, deviceScaleFactor });
        const errors = [];
        page.on('pageerror', e => errors.push(e.message));
        await page.evaluate(() => {
          window.messages = []; window.listeners = {};
          window.__JUCE__ = { backend: {
            emitEvent: (id,data) => messages.push(data),
            addEventListener: (id,fn) => listeners[id] = fn
          }};
        });
        await page.setContent(fs.readFileSync(path.join(__dirname,'../prototype/WUBROT.html'),'utf8'));
        const shell = await page.locator('.shell').boundingBox();
        assert(Math.abs(shell.width-width)<1 && shell.height<=height && height-shell.height<1);
        const scale = width/1200;
        const field = await page.locator('#field').boundingBox();
        // Preserve the original control scale, and verify XY after the height change.
        assert(Math.abs(field.width/width-.49532552)<.0001);
        await page.mouse.move(field.x+field.width/2,field.y+field.height/2);
        await page.mouse.down();
        await page.mouse.move(field.x+field.width*.6,field.y+field.height*.4);
        await page.mouse.up();
        assert(await page.evaluate(() => Math.abs(px-.1/.421875)<.01 && Math.abs(py+.1/.421875)<.01));
        const power = await page.locator('#enginePower').boundingBox();
        const preset = await page.locator('#preset').boundingBox();
        assert(power.x > preset.x+preset.width && power.y < preset.y+preset.height);
        assert(Math.abs(power.width-power.height)<1 && power.y+power.height < field.y);
        await page.locator('#enginePower').click();
        assert.equal(await page.locator('#engineStatus').textContent(),'ENGINE BYPASSED');
        assert(await page.evaluate(() => messages.some(m => m.type==='parameter' && m.id==='enabled' && m.value===0)));
        for (const enabled of [0,1]) {
          for (const status of ['DNA loaded.', 'Preset controls loaded; its factory WAV has not been supplied. Drop your own WAV into ROT DNA.', 'A long sample analysis diagnostic message '.repeat(12)]) {
            await page.evaluate(({enabled,status,index}) => {
              messages=[];
              listeners.wubrotState({enabled,status,editorSize:index,sampleRate:48000,x:0,y:0,mutate:67,drive:8.5,mode:0,preset:0,name:'very-long-factory-sample-name.wav'});
            }, {enabled,status,index});
            assert.equal(await page.locator('#engineStatus').textContent(),enabled?'ENGINE ACTIVE':'ENGINE BYPASSED');
            assert.equal(await page.locator('#enginePower').getAttribute('aria-pressed'),String(!!enabled));
            assert(await page.evaluate(() => messages.length===0));
            const boxes = await Promise.all(['#engineStatus','#texture','#motion'].map(s => page.locator(s).boundingBox()));
            const footer=await page.locator('.bottom').boundingBox(),grid=await page.locator('.grid').boundingBox();
            const geometry=JSON.stringify({width,height,deviceScaleFactor,enabled,statusLength:status.length,boxes,footer,grid});
            assert(grid.y+grid.height<=footer.y-4*scale,'main panels overlap footer: '+geometry);
            for (const box of boxes) {
              assert(box.y+box.height <= height-14*scale, 'footer intrudes into bottom border: '+geometry);
              assert(box.x>=20*scale && box.x+box.width<=width-20*scale);
            }
            assert(boxes[0].x+boxes[0].width<boxes[1].x && boxes[1].x+boxes[1].width<boxes[2].x);
            assert(await page.evaluate(() => document.documentElement.scrollWidth<=innerWidth && document.documentElement.scrollHeight<=innerHeight));
          }
        }
        await page.locator('#sizeSettings').click();
        const popup = await page.locator('#sizePopup').boundingBox();
        assert(popup.x>=0 && popup.y>=0 && popup.x+popup.width<=width && popup.y+popup.height<=height);
        assert.equal(await page.locator(`[data-size="${index}"]`).getAttribute('aria-pressed'),'true');
        await page.locator('[data-size="0"]').click();
        assert(await page.evaluate(() => messages.some(m => m.type==='uiSize' && m.index===0)));
        assert(await page.locator('#sizePopup').isHidden());
        await page.locator('#sizeSettings').click(); await page.keyboard.press('Escape');
        assert(await page.locator('#sizePopup').isHidden());
        assert.deepEqual(errors,[]);
        if (process.env.WUBROT_SCREENSHOTS && deviceScaleFactor===1)
          await page.screenshot({path:path.join(process.env.WUBROT_SCREENSHOTS,`wubrot-${width}.png`)});
        console.log(`${width}x${height} DPR ${deviceScaleFactor}: power, status, footer padding, long messages, popup and XY passed`);
        await page.close();
      }
    }
  } finally { await browser.close(); }
})().catch(e => { console.error(e); process.exit(1); });
