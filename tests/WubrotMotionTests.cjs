// Run with: node tests/WubrotMotionTests.cjs. No browser or dependencies required.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const html = fs.readFileSync(path.join(__dirname, '../prototype/WUBROT.html'), 'utf8');
const source = html.match(/<script id="gel-physics">([\s\S]*?)<\/script>/)[1];
const GelPhysics = vm.runInNewContext(source + '; GelPhysics');

function gesture(duration) {
    const gel = new GelPhysics();
    const steps = Math.round(duration * 120);
    let peak = 0;
    for (let i = 1; i <= steps; i++) {
        gel.impulse((i - 1) / steps * .8, 0, i / steps * .8, 0, 1 / 120);
        gel.advance(1 / 120);
        peak = Math.max(peak, gel.energy);
    }
    return { gel, peak };
}

const idle = new GelPhysics();
for (let i = 0; i < 600; i++) idle.advance(1 / 60);
assert.equal(idle.active, false, 'the jelly must not generate its own motion');
assert.ok(idle.h.every(value => value === 0), 'idle surface stays flat');
idle.impulse(.2, .4, .2, .4, .01);
assert.equal(idle.active, false, 'a stationary pointer must not keep the jelly awake');

const slow = gesture(1.2), fast = gesture(.15);
assert.ok(fast.peak > slow.peak * 3, 'the same path at higher speed must create a stronger response');
const releaseEnergy = fast.gel.energy;
fast.gel.advance(1 / 60);
assert.ok(fast.gel.active && fast.gel.energy > .02, 'momentum continues when pointer input stops');
const initialSign = Math.sign(fast.gel.x);
let overshoot = false, settleTime = 0;
while (fast.gel.active && settleTime < 3) {
    fast.gel.advance(1 / 120);
    settleTime += 1 / 120;
    if (Math.sign(fast.gel.x) === -initialSign) overshoot = true;
}
assert.ok(overshoot, 'the gel should wobble past rest instead of easing monotonically');
assert.ok(settleTime > .3 && settleTime < 2.5, 'a fast flick should settle after a brief tail');
assert.ok(releaseEnergy > fast.gel.energy);
assert.equal(fast.gel.active, false);
assert.ok(fast.gel.h.every(value => value === 0), 'sleep clears residual displacement');
assert.equal(fast.gel.x, 0, 'rest is neutral, independent of the orb position');

function atRefreshRate(hz) {
    const gel = new GelPhysics();
    gel.impulse(-.4, .2, .6, -.3, .1);
    for (let i = 0; i < hz / 2; i++) gel.advance(1 / hz);
    return gel;
}
const at60 = atRefreshRate(60);
for (const hz of [120, 144]) {
    const other = atRefreshRate(hz);
    assert.ok(Math.abs(at60.energy - other.energy) < .00001, `damping should match at ${hz} Hz`);
    for (let i = 0; i < at60.h.length; i++)
        assert.ok(Math.abs(at60.h[i] - other.h[i]) < .00001, 'wave propagation must not depend on display refresh');
}

const stressed = new GelPhysics();
for (let i = 0; i < 360; i++) {
    const x = i % 2 ? -.9 : .9;
    stressed.impulse(-x, -.2, x, .2, .004, 1);
    stressed.advance(1 / 120);
}
assert.ok(stressed.h.every(value => Number.isFinite(value) && Math.abs(value) < 50), 'rapid flicks remain stable');
stressed.advance(600);
assert.ok(stressed.h.every(Number.isFinite), 'a delayed frame cannot destabilise the simulation');
stressed.reset();
const packed = stressed.normals();
for (let i = 0; i < packed.length; i += 4) {
    assert.equal((packed[i] * 256 + packed[i + 1] - 32768) / 4096, 0);
    assert.equal((packed[i + 2] * 256 + packed[i + 3] - 32768) / 4096, 0);
}
console.log(`PASS: no idle drift, speed response, inertial overshoot, ${settleTime.toFixed(2)}s fast-flick settling, refresh independence, stability, neutral texture`);
