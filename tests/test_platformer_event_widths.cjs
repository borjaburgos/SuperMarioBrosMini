const assert = require('node:assert/strict');
const test = require('node:test');

const events = '../plugins/PlatformerPlus/events/';

function compile(file, input) {
  const writes = [];
  const helpers = {
    _addComment() {}, _addNL() {},
    _declareLocal() { return 'TMP'; },
    variableSetToScriptValue() {},
  };
  for (const name of ['_setConstMemInt16', '_setMemInt16ToVariable',
                       '_setConstMemUInt8', '_setMemUInt8ToVariable']) {
    helpers[name] = (...args) => writes.push([name, ...args]);
  }
  require(events + file).compile(input, helpers);
  return writes;
}

test('setting the upcoming player state writes one byte', () => {
  assert.deepEqual(compile('eventPPSetState.js', { state: { type: 'number', value: 12 } }),
    [['_setMemUInt8ToVariable', 'que_state', 'TMP']]);
});

test('detaching a platform writes only the attachment flag', () => {
  assert.deepEqual(compile('eventPPDetachPlayer.js', { field: 'actor_attached', state: '0' }),
    [['_setConstMemUInt8', 'actor_attached', '0']]);
});

test('field updates use byte counters and retain word-sized movement amounts', () => {
  for (const field of ['dj_val', 'nocollide', 'plat_hold_jump_max', 'jump_per_frame', 'boost_val']) {
    const suffix = ['jump_per_frame', 'boost_val'].includes(field) ? 'Int16' : 'UInt8';
    assert.deepEqual(compile('eventPPFieldSet.js', { field, type: 'number', value: 7 }),
      [['_setConstMem' + suffix, field, 7]]);
    assert.deepEqual(compile('eventPPFieldSet.js', { field, type: 'variable', variable: 'V0' }),
      [['_setMem' + suffix + 'ToVariable', field, 'V0']]);
  }
});
