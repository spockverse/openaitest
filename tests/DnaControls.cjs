const { chromium } = require(process.env.WUBROT_PLAYWRIGHT || 'playwright');
const fs = require('fs'), path = require('path'), assert = require('assert');
const html = fs.readFileSync(path.join(__dirname, '../prototype/WUBROT.html'), 'utf8');
const speeds = ['0.25×', '0.5×', '1×', '2×', '4×'];
async function checkSpeed(p, index) {
  await p.waitForFunction(index => document.getElementById('dnaSpeedKnob').getAttribute('aria-valuenow') === String(index), index);
  assert(await p.evaluate(() => scrollX === 0 && scrollY === 0 && document.querySelector('.shell').getBoundingClientRect().top === 0), 'Interaction scrolled the fixed editor');
  assert.equal(await p.locator('#dnaSpeedKnob').getAttribute('aria-valuenow'), String(index));
  assert.equal(await p.locator('#dnaSpeedKnob').getAttribute('aria-valuetext'), speeds[index]);
  assert.equal(await p.locator('#dnaSpeedValue').textContent(), speeds[index]);
  assert.equal(await p.locator('[data-speed][aria-selected=true]').getAttribute('data-speed'), String(index));
  assert.equal(await p.locator('#dnaSpeedKnob').evaluate(el => el.style.getPropertyValue('--turn')), `${index / 4 * 270}deg`);
}
async function recall(p, values) {
  await p.evaluate(values => { Object.assign(state, values); messages = []; listeners.wubrotState(state); }, values);
  assert.equal(await p.evaluate(() => messages.length), 0, 'Host state echoed into automation');
}
async function assertGesture(p) {
  const events = await p.evaluate(() => messages.filter(m => m.id === 'dnaSpeed'));
  assert(events.some(m => m.type === 'parameter'), 'No speed parameter event');
  assert.deepEqual(events.filter(m => m.type === 'gesture').map(m => m.start), [true, false]);
  assert.equal(events[0].type, 'gesture');
  assert.equal(events.at(-1).type, 'gesture');
}
(async () => {
  const b = await chromium.launch({
    ...(process.env.WUBROT_CHROMIUM ? { executablePath: process.env.WUBROT_CHROMIUM } : {}),
    args: ['--no-sandbox']
  });
  try {
    for (const dpr of [1, 1.5, 2]) for (const [width, height] of [[600, 520], [804, 697], [996, 864]]) {
      const scale = width / 1200;
      const p = await b.newPage({ viewport: { width, height }, deviceScaleFactor: dpr, reducedMotion: 'no-preference' });
      const errors = []; let fileDialogs = 0;
      p.on('pageerror', e => errors.push(e.message)); p.on('filechooser', () => fileDialogs++);
      await p.evaluate(() => {
        window.listeners = {}; window.messages = [];
        window.state = { enabled: 1, x: 0, y: 0, mutate: 67, drive: 8.5, mode: 0, regionStart: .25, regionEnd: .75,
          dnaSpeed: 2, editable: true, name: 'synthetic.wav', sampleRate: 48000, playhead: .4,
          status: 'Selected region shapes the distortion.', waveform: Array.from({ length: 128 }, (_, i) => .1 + .8 * Math.abs(Math.sin(i * .23))) };
        window.__JUCE__ = { backend: { addEventListener: (id, f) => listeners[id] = f, emitEvent: (id, e) => {
          messages.push(e); if (e.type === 'ready') listeners.wubrotState(state);
        } } };
      });
      await p.setContent(html);
      assert.equal(await p.locator('#dnaLoop,#dnaRetrigger,.dna-mode,#speedDown,#speedUp,select#dnaSpeed').count(), 0);
      assert.equal(await p.locator('.dna-speed-knob').count(), 1);
      assert.equal(await p.locator('#regionLoop').count(), 1);
      await checkSpeed(p, 2);
      const selectors = ['.region-nudge', '#dnaSpeedKnob', '#speedSelect', '#regionLoop'];
      const row = await p.locator('#regionToolbar').boundingBox();
      const drop = await p.locator('#drop').boundingBox();
      let previous = null;
      for (const sel of selectors) {
        const box = await p.locator(sel).boundingBox();
        assert(box.x >= row.x - .1 && box.x + box.width <= row.x + row.width + .1, `${sel} exceeds row`);
        assert(box.y >= row.y - .1 && box.y + box.height <= row.y + row.height + .1, `${sel} exceeds row height`);
        if (previous) assert(previous.x + previous.width + 2 * scale < box.x, `${sel} overlaps preceding control`);
        previous = box;
      }
      const marker = await p.locator('#regionEnd').boundingBox();
      assert(marker.y + marker.height + 2 * scale < row.y, 'Controls crowd START/END');
      const footer = await p.locator('.sample-footer').boundingBox(), engine = await p.locator('.engine').boundingBox();
      assert(row.y + row.height < drop.y + drop.height && drop.y + drop.height < footer.y && footer.y + footer.height < engine.y);
      assert(engine.y + engine.height <= (await p.locator('.bottom').boundingBox()).y + .1, 'New controls push engine into footer');

      // All supported choices use the existing parameter and a single closed gesture.
      for (let i = 0; i < 5; i++) {
        await p.evaluate(() => messages = []);
        await p.locator('#dnaSpeed').click();
        await p.waitForTimeout(200);
        const popup = await p.locator('#dnaSpeedMenu').boundingBox();
        assert(popup.x >= drop.x && popup.x + popup.width <= drop.x + drop.width);
        assert(popup.y >= drop.y && popup.y + popup.height <= drop.y + drop.height, 'Speed popup clipped');
        assert.equal(await p.locator('#dnaSpeedMenu').evaluate(el => getComputedStyle(el).transitionDuration.split(',')[0]), '0.17s');
        await p.locator(`[data-speed="${i}"]`).click(); await checkSpeed(p, i); await assertGesture(p);
        assert.equal(await p.locator('#dnaSpeed').getAttribute('aria-expanded'), 'false');
      }
      await p.locator('#dnaSpeed').click(); await p.keyboard.press('Escape');
      assert.equal(await p.locator('#dnaSpeed').getAttribute('aria-expanded'), 'false');
      await p.locator('#dnaSpeed').click(); await p.locator('.dna h2').click();
      assert.equal(await p.locator('#dnaSpeed').getAttribute('aria-expanded'), 'false');
      await p.locator('#dnaSpeed').focus(); await p.keyboard.press('ArrowDown');
      await p.keyboard.press('Home'); await p.keyboard.press('ArrowDown'); await p.keyboard.press('Enter'); await checkSpeed(p, 1);

      // A full drag brackets its steps, ignores stale host echoes while touched,
      // and measures deltas in design coordinates at every editor size/DPI.
      await recall(p, { dnaSpeed: 2 });
      const knob = await p.locator('#dnaSpeedKnob').boundingBox(), x = knob.x + knob.width / 2, y = knob.y + knob.height / 2;
      await p.mouse.move(x, y); await p.mouse.down(); await p.mouse.move(x, y - 34 * scale, { steps: 5 });
      await checkSpeed(p, 3);
      await p.evaluate(() => listeners.wubrotState({ ...state, dnaSpeed: 0 })); await checkSpeed(p, 3);
      await p.mouse.move(x, y - 66 * scale, { steps: 5 }); await p.mouse.up(); await checkSpeed(p, 4); await assertGesture(p);
      await recall(p, { dnaSpeed: 2 });
      await p.keyboard.down('Shift'); await p.mouse.move(x, y); await p.mouse.down();
      await p.mouse.move(x, y - 34 * scale, { steps: 5 }); await checkSpeed(p, 2);
      await p.mouse.move(x, y - 86 * scale, { steps: 5 }); await p.mouse.up(); await p.keyboard.up('Shift');
      await checkSpeed(p, 3); await assertGesture(p);
      await p.evaluate(() => messages = []); await p.mouse.move(x, y); await p.mouse.wheel(0, 100);
      await checkSpeed(p, 2); await assertGesture(p);
      await p.locator('#dnaSpeedKnob').focus(); await p.keyboard.press('Home'); await checkSpeed(p, 0);
      await p.keyboard.press('ArrowRight'); await checkSpeed(p, 1); await p.keyboard.press('End'); await checkSpeed(p, 4);
      await p.locator('#dnaSpeedKnob').dblclick(); await checkSpeed(p, 2);

      // One cyan switch: LOOP left, RETRIGGER right, same illuminated color.
      await recall(p, { mode: 0, dnaSpeed: 0 });
      const litColor = await p.locator('.mode-loop').evaluate(el => getComputedStyle(el).color);
      assert.equal(await p.locator('#regionLoop').getAttribute('aria-checked'), 'false');
      assert.equal(await p.locator('#regionLoop i').evaluate(el => getComputedStyle(el, ':after').transform), 'matrix(1, 0, 0, 1, 0, 0)');
      await p.locator('#regionLoop').click();
      // A busy Windows software renderer can present the final transition frame
      // after a fixed sleep; observe completion rather than sampling mid-flight.
      await p.waitForFunction(() => getComputedStyle(document.querySelector('#regionLoop i'), ':after').transform === 'matrix(1, 0, 0, 1, 16, 0)');
      assert.equal(await p.locator('#regionLoop').getAttribute('aria-checked'), 'true');
      assert.equal(await p.locator('.mode-retrigger').evaluate(el => getComputedStyle(el).color), litColor);
      assert.equal(await p.locator('#regionLoop i').evaluate(el => getComputedStyle(el, ':after').transform), 'matrix(1, 0, 0, 1, 16, 0)');
      assert(await p.evaluate(() => messages.some(m => m.type === 'parameter' && m.id === 'mode' && m.value === 1)));
      for (let i = 0; i < 5; i++) { await recall(p, { dnaSpeed: i, mode: i % 2 }); await checkSpeed(p, i); assert.equal(await p.locator('#regionLoop').getAttribute('aria-checked'), String(!!(i % 2))); }

      // OFF retains operable controls, neutral lighting, profile and parameters.
      await recall(p, { enabled: 0, dnaSpeed: 4, mode: 1 }); await p.waitForTimeout(300); await checkSpeed(p, 4);
      assert.equal(await p.locator('#dnaSpeedKnob span').evaluate(el => getComputedStyle(el, ':after').boxShadow), 'none');
      assert.equal(await p.locator('#regionLoop i').evaluate(el => getComputedStyle(el, ':after').boxShadow), 'none');
      await p.locator('#dnaSpeed').click(); await p.waitForTimeout(210);
      assert.equal(await p.locator('#dnaSpeedMenu').evaluate(el => getComputedStyle(el).boxShadow), 'none');
      if (process.env.WUBROT_SCREENSHOTS && dpr === 1.5) await p.screenshot({ path: path.join(process.env.WUBROT_SCREENSHOTS, `dna-controls-off-${width}.png`) });
      await p.locator('[data-speed="1"]').click(); await checkSpeed(p, 1);
      await p.locator('#regionLoop').click(); assert.equal(await p.locator('#regionLoop').getAttribute('aria-checked'), 'false');
      assert.equal(await p.locator('#enginePower').getAttribute('aria-pressed'), 'false');
      await recall(p, { enabled: 1, dnaSpeed: 1, mode: 0 });
      await p.locator('#dnaSpeed').click(); await p.waitForTimeout(210);
      assert(await p.evaluate(() => scrollX === 0 && scrollY === 0 && document.querySelector('.shell').getBoundingClientRect().top === 0), 'Opening speed menu shifted the editor');
      if (process.env.WUBROT_SCREENSHOTS && dpr === 1.5) await p.screenshot({ path: path.join(process.env.WUBROT_SCREENSHOTS, `dna-controls-on-${width}.png`) });
      await p.keyboard.press('Escape');
      await p.emulateMedia({ reducedMotion: 'reduce' });
      assert.equal(await p.locator('#dnaSpeedMenu').evaluate(el => getComputedStyle(el).transitionDuration), '0s');
      assert.equal(fileDialogs, 0, 'Control interactions opened the sample picker'); assert.deepEqual(errors, []);
      console.log(`${width} @ ${dpr}: layout, custom options/keys, drag/fine/wheel, host echo/gestures, mode and OFF passed`);
      await p.close();
    }
  } finally { await b.close(); }
})().catch(e => { console.error(e); process.exit(1); });
