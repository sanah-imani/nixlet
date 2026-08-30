import { Terminal } from '@xterm/xterm'
import { FitAddon } from '@xterm/addon-fit'
import { WebLinksAddon } from '@xterm/addon-web-links'
import '@xterm/xterm/css/xterm.css'
import { loadNixlet } from './wasm-loader'

const term = new Terminal({
  cursorBlink: true,
  fontFamily: '"Cascadia Code", "Fira Code", "JetBrains Mono", monospace',
  fontSize: 14,
  theme: {
    background:  '#0d0d0d',
    foreground:  '#e0e0e0',
    cursor:      '#00ff88',
    black:       '#1a1a1a',
    brightBlack: '#555555',
    green:       '#00ff88',
    brightGreen: '#00ffaa',
    cyan:        '#00d4ff',
    brightCyan:  '#44eeff',
  },
})

const fitAddon = new FitAddon()
term.loadAddon(fitAddon)
term.loadAddon(new WebLinksAddon())

const container = document.getElementById('terminal')!
term.open(container)
fitAddon.fit()

window.addEventListener('resize', () => fitAddon.fit())

// ── boot ──────────────────────────────────────────────────────────────────────

async function boot() {
  term.writeln('\x1b[1;32mnixlet\x1b[0m v0.1.0 — a tiny Unix-like environment')
  term.writeln('Loading kernel...')

  let nixlet: Awaited<ReturnType<typeof loadNixlet>>

  try {
    nixlet = await loadNixlet()
    nixlet.init()
    term.writeln('\x1b[32mReady.\x1b[0m\r\n')
  } catch (e) {
    term.writeln(`\x1b[31mFailed to load WASM module: ${e}\x1b[0m`)
    term.writeln('(Run \x1b[33mmake build\x1b[0m first to compile nixlet.wasm)')
    prompt()
    return
  }

  prompt()

  let inputBuffer = ''

  term.onKey(({ key, domEvent }) => {
    const code = domEvent.keyCode

    if (code === 13) {
      // Enter
      term.writeln('')
      const line = inputBuffer.trim()
      inputBuffer = ''

      if (line) {
        const output = nixlet.input(line)
        if (output) {
          // Normalize newlines for xterm (\n -> \r\n)
          term.write(output.replace(/\n/g, '\r\n'))
          if (!output.endsWith('\n')) term.writeln('')
        }
      }

      prompt()
    } else if (code === 8) {
      // Backspace
      if (inputBuffer.length > 0) {
        inputBuffer = inputBuffer.slice(0, -1)
        term.write('\b \b')
      }
    } else if (key.length === 1) {
      inputBuffer += key
      term.write(key)
    }
  })
}

function prompt() {
  term.write('\x1b[1;36mnixlet\x1b[0m:\x1b[33m~\x1b[0m$ ')
}

boot()
