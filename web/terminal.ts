import { Terminal } from '@xterm/xterm'
import { FitAddon } from '@xterm/addon-fit'
import { WebLinksAddon } from '@xterm/addon-web-links'
import '@xterm/xterm/css/xterm.css'
import { loadNixlet, NixletAPI } from './wasm-loader'

const VFS_STORAGE_KEY = 'nixlet_vfs'
const HISTORY_MAX     = 500

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

function fmtCwd(cwd: string): string {
  if (cwd === '/home/user' || cwd === '/home/user/') return '~'
  if (cwd.startsWith('/home/user/')) return '~' + cwd.slice('/home/user'.length)
  return cwd
}

function prompt(nixlet: NixletAPI) {
  const cwd = fmtCwd(nixlet.cwd())
  term.write(`\x1b[1;36mnixlet\x1b[0m:\x1b[33m${cwd}\x1b[0m$ `)
}

async function boot() {
  term.writeln('\x1b[1;32mnixlet\x1b[0m v0.1.0 — a tiny Unix-like environment')
  term.writeln('Loading kernel...')

  let nixlet: NixletAPI

  try {
    nixlet = await loadNixlet()
    nixlet.init()

    const saved = localStorage.getItem(VFS_STORAGE_KEY)
    if (saved) nixlet.deserialize(saved)

    window.addEventListener('beforeunload', () => {
      localStorage.setItem(VFS_STORAGE_KEY, nixlet.serialize())
    })

    term.writeln('\x1b[32mReady.\x1b[0m\r\n')
  } catch (e) {
    term.writeln(`\x1b[31mFailed to load WASM module: ${e}\x1b[0m`)
    term.writeln('(Run \x1b[33mmake build\x1b[0m first to compile nixlet.wasm)')
    term.write('\x1b[1;36mnixlet\x1b[0m:\x1b[33m~\x1b[0m$ ')
    return
  }

  prompt(nixlet)

  let inputBuffer = ''         // current line being typed
  let cursorPos   = 0          // position within inputBuffer

  // History
  const history: string[] = []
  let historyIdx = -1          
  let historySaved = ''        // saves current draft when browsing up


  // Redraw the input line from scratch after cursor/buffer changes
  function redrawInput(newBuffer: string, newCursor: number) {
    // Move to start of input, clear to end of line, rewrite buffer, reposition cursor
    const oldLen = inputBuffer.length
    // Move cursor back to start of input area
    if (cursorPos > 0) term.write(`\x1b[${cursorPos}D`)
    term.write('\x1b[K')  // clear to end of line
    term.write(newBuffer)
    // Move cursor to correct position
    const delta = newBuffer.length - newCursor
    if (delta > 0) term.write(`\x1b[${delta}D`)
    inputBuffer = newBuffer
    cursorPos   = newCursor
  }

  function submitLine() {
    term.writeln('')
    const line = inputBuffer.trim()
    inputBuffer = ''
    cursorPos   = 0
    historyIdx  = -1
    historySaved = ''

    if (line) {
      // Push to history (avoid consecutive duplicates)
      if (history[history.length - 1] !== line) {
        history.push(line)
        if (history.length > HISTORY_MAX) history.shift()
      }

      const output = nixlet.input(line)
      if (output && output !== '\x00EXIT') {
        term.write(output.replace(/\n/g, '\r\n'))
        if (!output.endsWith('\n')) term.writeln('')
      }
    }

    prompt(nixlet)
  }

  // ── key handler ──────────────────────────────────────────────────────────────

  term.onKey(({ key, domEvent }) => {
    const code = domEvent.keyCode

    // Enter
    if (code === 13) {
      submitLine()
      return
    }

    // Ctrl+C — cancel current input
    if (domEvent.ctrlKey && code === 67) {
      term.writeln('^C')
      inputBuffer  = ''
      cursorPos    = 0
      historyIdx   = -1
      historySaved = ''
      prompt(nixlet)
      return
    }

    // Ctrl+L — clear screen
    if (domEvent.ctrlKey && code === 76) {
      term.clear()
      prompt(nixlet)
      return
    }

    // Backspace
    if (code === 8) {
      if (cursorPos > 0) {
        const nb = inputBuffer.slice(0, cursorPos - 1) + inputBuffer.slice(cursorPos)
        redrawInput(nb, cursorPos - 1)
      }
      return
    }

    // Delete
    if (code === 46) {
      if (cursorPos < inputBuffer.length) {
        const nb = inputBuffer.slice(0, cursorPos) + inputBuffer.slice(cursorPos + 1)
        redrawInput(nb, cursorPos)
      }
      return
    }

    // Arrow left
    if (code === 37) {
      if (cursorPos > 0) {
        cursorPos--
        term.write('\x1b[D')
      }
      return
    }

    // Arrow right
    if (code === 39) {
      if (cursorPos < inputBuffer.length) {
        cursorPos++
        term.write('\x1b[C')
      }
      return
    }

    // Arrow up — history back
    if (code === 38) {
      if (history.length === 0) return
      if (historyIdx === -1) {
        historySaved = inputBuffer
        historyIdx   = history.length - 1
      } else if (historyIdx > 0) {
        historyIdx--
      }
      const entry = history[historyIdx]
      redrawInput(entry, entry.length)
      return
    }

    // Arrow down — history forward
    if (code === 40) {
      if (historyIdx === -1) return
      if (historyIdx < history.length - 1) {
        historyIdx++
        const entry = history[historyIdx]
        redrawInput(entry, entry.length)
      } else {
        historyIdx = -1
        redrawInput(historySaved, historySaved.length)
        historySaved = ''
      }
      return
    }

    // Home — move to start
    if (code === 36) {
      if (cursorPos > 0) {
        term.write(`\x1b[${cursorPos}D`)
        cursorPos = 0
      }
      return
    }

    // End — move to end
    if (code === 35) {
      const delta = inputBuffer.length - cursorPos
      if (delta > 0) {
        term.write(`\x1b[${delta}C`)
        cursorPos = inputBuffer.length
      }
      return
    }

    // Printable characters
    if (key.length === 1 && !domEvent.ctrlKey && !domEvent.altKey) {
      const nb = inputBuffer.slice(0, cursorPos) + key + inputBuffer.slice(cursorPos)
      redrawInput(nb, cursorPos + 1)
      return
    }
  })
}

boot()
