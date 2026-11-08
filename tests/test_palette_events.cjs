const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const test = require('node:test');

const paletteEvents = path.join(__dirname, '../plugins/setPaletteColorPlugin/events');

function loadCompiler(filename) {
  const context = {};
  vm.runInNewContext(fs.readFileSync(path.join(paletteEvents, filename), 'utf8')
    .replace(/^export /gm, '') + '\nglobalThis.compileEvent = compile;', context);
  return context.compileEvent;
}

const getColors = loadCompiler('eventGetPaletteColors.js');
const colors = [0x1231, 0x2342, 0x3453, 0x4564];

function getterModel(mask, sprite) {
  const memory = new Map([[0, 0xCAFE]]);
  const stack = [];
  const nativeCalls = [];
  const aliasesRead = [];
  const inputs = Array.from({ length: 4 }, (_, i) =>
    `${mask & (1 << i) ? 'PARAM' : 'GLOBAL'}${i}`);
  const alias = (variable) => `SLOT_${variable}`;
  inputs.forEach((variable, i) => {
    const indirect = variable.startsWith('PARAM');
    memory.set(alias(variable), indirect ? 1000 + i : -1);
    if (indirect) memory.set(1000 + i, -1);
  });
  const helpers = {
    _addComment() {},
    _declareLocal(name) {
      const variable = `LOCAL_${name}`;
      memory.set(variable, 0);
      return variable;
    },
    variableSetToScriptValue(variable, value) { memory.set(variable, value.value); },
    getVariableAlias(variable) { aliasesRead.push(variable); return alias(variable); },
    _isIndirectVariable(variable) { return variable.startsWith('PARAM'); },
    _stackPushConst(value) { stack.push(value); },
    _stackPush(variable) { stack.push(memory.get(variable)); },
    _stackPop(count) { stack.splice(-count); },
    _setInd(pointer, source) { memory.set(memory.get(pointer), memory.get(source)); },
    _rpn() {
      const operands = [];
      let destination;
      const rpn = {
        ref(variable) { operands.push(memory.get(variable)); return rpn; },
        int16(value) { operands.push(value); return rpn; },
        operator(operation) {
          assert.equal(operation, '.B_OR');
          const right = operands.pop();
          operands.push(operands.pop() | right);
          return rpn;
        },
        refSet(variable) { destination = variable; return rpn; },
        stop() { memory.set(destination, operands.pop()); },
      };
      return rpn;
    },
    _callNative(name) {
      assert.equal(name, 'get_palette_colors');
      assert.equal(stack.length, 5);
      nativeCalls.push(name);
      // Native arguments are palette flags followed by four destination slots.
      // A parameter slot holds a pointer; writing it directly corrupts that
      // pointer. Correct compilation writes a local and copies it indirectly.
      assert.equal(stack[4], sprite ? 8 : 0);
      for (let i = 0; i < (sprite ? 3 : 4); i++) {
        memory.set(stack[3 - i], colors[i]);
      }
    },
  };
  const input = { is_gbc: true, is_sprite: sprite, palette_idx: { type: 'number', value: 0 } };
  inputs.forEach((variable, i) => { input[`color${i}`] = variable; });
  getColors(input, helpers);
  assert.deepEqual(nativeCalls, ['get_palette_colors']);
  assert.equal(stack.length, 0);
  assert.equal(memory.get(0), 0xCAFE, 'Sprite transparency must not overwrite variable zero');
  inputs.forEach((variable, i) => {
    const indirect = variable.startsWith('PARAM');
    const output = indirect ? 1000 + i : alias(variable);
    const expected = sprite && i === 3 ? -1 : colors[i];
    assert.equal(memory.get(output), expected, `Color ${i + 1} output (${variable})`);
    if (indirect) assert.equal(memory.get(alias(variable)), 1000 + i, 'Parameter pointer was overwritten');
  });
  if (sprite) assert.ok(!aliasesRead.includes(inputs[3]), 'Sprite Color 4 is not an output');
}

for (const sprite of [false, true]) {
  for (let mask = 0; mask < 16; mask++) {
    test(`palette getter ${sprite ? 'sprite' : 'background'} outputs ${mask.toString(2).padStart(4, '0')} (1=parameter)`, () => {
      getterModel(mask, sprite);
    });
  }
}
