export interface NixletAPI {
  init(): void;
  input(line: string): string;
  cwd(): string;
  serialize(): string;
  deserialize(json: string): boolean;
  free(): void;
}

let _module: any = null;
let _init:        () => void;
let _input:       (line: string) => number;
let _cwd:         () => number;
let _serialize:   () => number;
let _deserialize: (json: string) => number;
let _free:        () => void;

export async function loadNixlet(): Promise<NixletAPI> {
  const { default: NixletModule } = await import('../build/nixlet.js')
  _module = await NixletModule()

  _init        = _module.cwrap('nixlet_init',        null,     [])
  _input       = _module.cwrap('nixlet_input',       'number', ['string'])
  _cwd         = _module.cwrap('nixlet_cwd',         'number', [])
  _serialize   = _module.cwrap('nixlet_serialize',   'number', [])
  _deserialize = _module.cwrap('nixlet_deserialize', 'number', ['string'])
  _free        = _module.cwrap('nixlet_free',        null,     [])

  return {
    init() {
      _init()
    },
    input(line: string): string {
      const ptr = _input(line)
      return ptr ? _module.UTF8ToString(ptr) : ''
    },
    cwd(): string {
      const ptr = _cwd()
      return ptr ? _module.UTF8ToString(ptr) : '/'
    },
    serialize(): string {
      const ptr = _serialize()
      return ptr ? _module.UTF8ToString(ptr) : ''
    },
    deserialize(json: string): boolean {
      return _deserialize(json) !== 0
    },
    free() {
      _free()
    },
  }
}
