// Scene maps occupy SRAM bank 0 immediately before the collision lookup table.
const MAP_BYTES_8 = 0x1f00;
const MAP_BYTES_16 = 0x1c00;

const validateMetatileMaps = (options) => {
  const configured = (options.engineFieldValues || []).find(
    (field) => field.id === "METATILE_SIZE"
  );
  const mode = configured?.value ?? options.engineFields?.METATILE_SIZE?.defaultValue;
  const is16px = mode === "METATILE_SIZE_16" || mode === 1 || mode === "1";
  // This cache belongs to one GB Studio compilation, whose scene resources
  // are immutable. Shared InitLevel instances must not rescan every scene.
  const cache = options.compiledAssetsCache;
  const cacheKey = `__metatileMapCapacityValidated_${is16px ? 16 : 8}`;
  if (cache?.[cacheKey]) return;
  const capacity = is16px ? MAP_BYTES_16 : MAP_BYTES_8;
  const scenes = options.scenes || [];
  const sceneIds = new Set(scenes.map((scene) => scene.id));
  const customEvents = new Map((options.customEvents || []).map(
    (script) => [script.id, script]
  ));

  const validateDimensions = (scene) => {
    // Compiled backgrounds are authoritative when a source image was resized.
    const background = scene.background || (options.backgrounds || []).find(
      (image) => image.id === scene.backgroundId
    );
    const width = background?.width ?? scene.width;
    const height = background?.height ?? scene.height;
    const name = scene.name || scene.symbol || scene.id;
    if (!Number.isInteger(width) || !Number.isInteger(height) ||
        width <= 0 || height <= 0 || width > 255 || height > 255 ||
        (is16px && ((width & 1) || (height & 1)))) {
      throw new Error(
        `Metatile map "${name}" has unsupported dimensions ${width}x${height} tiles. ` +
        (is16px ? "16px metatiles require positive even dimensions up to 254 tiles per axis. " :
          "Dimensions must be between 1 and 255 tiles per axis. ") +
        `The map storage limit is ${capacity} bytes.`
      );
    }

    let bytes = width * height;
    if (is16px) {
      const halfWidth = width / 2;
      const halfHeight = height / 2;
      // Match image_tile_width_bit, including its minimum two-byte stride.
      const stride = Math.max(2, 2 ** Math.ceil(Math.log2(halfWidth)));
      bytes = (halfHeight - 1) * stride + halfWidth;
    }
    if (bytes > capacity) {
      throw new Error(
        `Metatile map "${name}" is ${width}x${height} tiles and requires ${bytes} bytes; ` +
        `the ${is16px ? 16 : 8}px metatile map limit is ${capacity} bytes. ` +
        "Reduce this scene's background dimensions."
      );
    }
  };

  const containsLoad = (value, visitedScripts) => {
    if (!value || typeof value !== "object") return false;
    // GB Studio passes proxied arrays into plugin handlers. Traverse arrays
    // directly: Object.entries() cannot enumerate those sandbox proxies.
    if (Array.isArray(value)) return value.some((event) => containsLoad(event, visitedScripts));
    if (!value.command) return false;
    if (value.args?.__comment) return false;
    if (value.command === "EVENT_LOAD_META_TILES" && sceneIds.has(value.args?.sceneId)) {
      return true;
    }
    if (value.command === "EVENT_CALL_CUSTOM_EVENT") {
      const id = value.args?.customEventId;
      const script = customEvents.get(id);
      if (script && !visitedScripts.has(id)) {
        visitedScripts.add(id);
        if (containsLoad(script.script, visitedScripts)) return true;
      }
    }
    return Object.values(value.children || {}).some((child) => containsLoad(child, visitedScripts));
  };

  const scriptKeys = ["script", "startScript", "updateScript", "leaveScript",
    "hit1Script", "hit2Script", "hit3Script", "playerHit1Script",
    "playerHit2Script", "playerHit3Script"];
  const sceneUsesMetatiles = (scene) => {
    const visitedScripts = new Set();
    const entityUsesMetatiles = (entity) => scriptKeys.some(
      (key) => containsLoad(entity[key], visitedScripts)
    );
    return entityUsesMetatiles(scene) || (scene.actors || []).some(entityUsesMetatiles) ||
      (scene.triggers || []).some(entityUsesMetatiles);
  };

  // Shared custom events are compiled once and cached. Checking only the
  // current scene would miss larger scenes which call the same InitLevel.
  if (options.scene) validateDimensions(options.scene);
  for (const scene of scenes) {
    if (scene.id !== options.scene?.id && sceneUsesMetatiles(scene)) {
      validateDimensions(scene);
    }
  }
  if (cache) cache[cacheKey] = true;
};

export const id = "EVENT_LOAD_META_TILES";
export const name = "Load meta tiles";
export const groups = ["Meta Tiles"];

export const autoLabel = (fetchArg) => {
  return `Load meta tiles`;
};

export const fields = [
  {
    key: "sceneId",
    label: "Metatile Scene",
    type: "scene",
	width: "100%",
    defaultValue: "LAST_SCENE",
  },
];

export const compile = (input, helpers) => {
  const { options, _callNative, _stackPushConst, _stackPush, _stackPop, _addComment, _declareLocal, variableSetToScriptValue } = helpers;
  
  const { scenes } = options;
  const scene = scenes.find((s) => s.id === input.sceneId);
  if (!scene) {
    return;
  }

  validateMetatileMaps(options);
    
  _addComment("Load meta tiles");
  
  _stackPushConst(`_${scene.symbol}`);
  _stackPushConst(`___bank_${scene.symbol}`);
  		
  _callNative("vm_load_meta_tiles");
  _stackPop(2);  
  
};
