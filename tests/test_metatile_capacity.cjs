const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { createRequire } = require('node:module');
const vm = require('node:vm');
const test = require('node:test');

const eventPath = path.join(__dirname, '../plugins/DynamicMapPlugin/events/eventLoadMetaTiles.js');
const context = { require: createRequire(eventPath) };
vm.runInNewContext(fs.readFileSync(eventPath, 'utf8').replace(/^export /gm, '') +
  '\nglobalThis.compileEvent = compile;', context);

const load = { command: 'EVENT_LOAD_META_TILES', args: { sceneId: 'atlas' } };
const call = (id) => ({ command: 'EVENT_CALL_CUSTOM_EVENT', args: { customEventId: id } });
const scene = (name, width, height, script = [load]) => ({
  id: name, name, symbol: name, width, height, script,
});

function compile(current, { scenes = [], mode, defaultMode, customEvents = [] } = {}) {
  const writes = [];
  const atlas = scenes.find((s) => s.id === 'atlas') || scene('atlas', 32, 32, []);
  const allScenes = [current, ...scenes.filter((s) => s.id !== current.id)];
  if (!allScenes.some((s) => s.id === 'atlas')) allScenes.push(atlas);
  context.compileEvent({ sceneId: 'atlas' }, {
    options: {
      scene: current, scenes: allScenes, customEvents,
      engineFieldValues: mode === undefined ? [] : [{ id: 'METATILE_SIZE', value: mode }],
      engineFields: defaultMode ? { METATILE_SIZE: { defaultValue: defaultMode } } : {},
    },
    _addComment() {}, _stackPushConst(value) { writes.push(value); },
    _callNative(value) { writes.push(value); }, _stackPop() {},
  });
  return writes;
}

test('8px maps accept the exact SRAM limit and current World 16 dimensions', () => {
  for (const [w, h] of [[208, 32], [208, 38], [248, 32]]) {
    assert.deepEqual(compile(scene('Allowed map', w, h)),
      ['_atlas', '___bank_atlas', 'vm_load_meta_tiles']);
  }
});

test('oversized 8px maps fail with scene, dimensions, size, and limit', () => {
  for (const [h, bytes] of [[39, 8112], [40, 8320]]) {
    assert.throws(() => compile(scene('16-1 expanded', 208, h)), (error) => {
      for (const text of ['16-1 expanded', `208x${h}`, String(bytes), '7936']) {
        assert.ok(error.message.includes(text), error.message);
      }
      return true;
    });
  }
});

test('checks the active background map rather than its metatile dictionary atlas', () => {
  assert.throws(() => compile(scene('Large level', 208, 39), {
    scenes: [scene('atlas', 16, 16, [])],
  }), /Large level/);
  assert.doesNotThrow(() => compile(scene('Small level', 20, 18), {
    scenes: [scene('atlas', 255, 255, []), scene('Static large image', 255, 255, [])],
  }));
});

test('resized compiled background dimensions take precedence over stale scene dimensions', () => {
  const current = scene('Resized background', 20, 18);
  current.background = { width: 208, height: 39 };
  assert.throws(() => compile(current), /Resized background.*208x39/);
});

test('shared InitLevel custom events validate all calling scenes despite compiler caching', () => {
  const init = { id: 'init', script: [load] };
  const current = scene('1-1', 216, 18, [call('init')]);
  const expanded = scene('16-1 expanded', 208, 39, [call('init')]);
  assert.throws(() => compile(current, { scenes: [expanded], customEvents: [init] }),
    /16-1 expanded.*8112.*7936/);
});

test('nested custom events and cycles do not hide map users', () => {
  const customEvents = [
    { id: 'outer', script: [call('inner'), load] },
    { id: 'inner', script: [call('outer')] },
  ];
  assert.throws(() => compile(scene('Small', 20, 18), {
    scenes: [scene('Nested oversized', 208, 40, [call('inner')])], customEvents,
  }), /Nested oversized/);
});

test('16px maps count padded rows and permit the exact SRAM limit', () => {
  for (const mode of ['METATILE_SIZE_16', 1, '1']) {
    assert.doesNotThrow(() => compile(scene('Exact 16px limit', 128, 224), { mode }));
    assert.doesNotThrow(() => compile(scene('Padded fits', 130, 112), { mode }));
    assert.throws(() => compile(scene('Padded overflow', 130, 114), { mode }),
      /Padded overflow.*7233.*7168/);
    assert.throws(() => compile(scene('Too tall', 128, 226), { mode }), /7232.*7168/);
  }
});

test('16px mode uses engine defaults and rejects odd or zero dimensions', () => {
  assert.throws(() => compile(scene('Odd map', 21, 18), {
    defaultMode: 'METATILE_SIZE_16',
  }), /Odd map.*21x18.*positive even/);
  for (const [w, h] of [[0, 18], [20, 0], [256, 18]]) {
    assert.throws(() => compile(scene('Invalid map', w, h)), /Invalid map.*unsupported dimensions/);
  }
});

test('all existing map users pass the build-time capacity check', () => {
  const project = path.join(__dirname, '../project');
  const scenes = fs.readdirSync(path.join(project, 'scenes')).map((name) =>
    JSON.parse(fs.readFileSync(path.join(project, 'scenes', name, 'scene.gbsres'), 'utf8')));
  const customEvents = fs.readdirSync(path.join(project, 'scripts')).filter((name) =>
    name.endsWith('.gbsres')).map((name) =>
    JSON.parse(fs.readFileSync(path.join(project, 'scripts', name), 'utf8')));
  assert.doesNotThrow(() => compile(scenes.find((s) => s.name === '1-1'), {
    scenes, customEvents,
  }));
});
