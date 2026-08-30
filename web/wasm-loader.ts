// Loads the Emscripten-generated WASM module and wraps the C API.

export interface NixletAPI {
  init(): void;
  input(line: string): string;
  free(): void;
}

let _module: any = null;
let _init: () => void;
let _input: (ptr: number, len: number) => number;
let _free: () => void;

export async function loadNixlet(): Promise<NixletAPI> {
  // Dynamically import the Emscripten glue JS
  const { default: NixletModule } = await import('../build/nixlet.js')

  _module = await NixletModule()

  _init  = _module.cwrap('nixlet_init',  null,     [])
  _input = _module.cwrap('nixlet_input', 'number', ['string'])
  _free  = _module.cwrap('nixlet_free',  null,     [])

  return {
    init() {
      _init()
    },
    input(line: string): string {
      // nixlet_input returns a char* (owned by the C side, valid until next call)
      const ptr = _input(line)
      return ptr ? _module.UTF8ToString(ptr) : ''
    },
    free() {
      _free()
    },
  }
}
