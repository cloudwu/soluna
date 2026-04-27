import { readdir, readFile, stat } from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

export interface ExampleRuntimeFile {
  path: string
  source: string
}

export interface ExampleEntry {
  id: string
  title: string
  entry: string
  source: string
  gameSettings: string
  runtimeFiles: ExampleRuntimeFile[]
}

export interface DocBlock {
  signature: string | null
  docs: string[]
  annos: string[]
}

export interface DocEntry {
  id: string
  module: string
  title: string
  blocks: DocBlock[]
}

const repoRoot = path.resolve(fileURLToPath(new URL('../../..', import.meta.url)))
const testDir = path.join(repoRoot, 'test')
const docsDir = path.join(repoRoot, 'docs')

function trim(value: string): string {
  return value.trim()
}

export function titleize(name: string): string {
  return name
    .split(/[_\-\s]+/)
    .filter(Boolean)
    .map(part => part.slice(0, 1).toUpperCase() + part.slice(1))
    .join(' ')
}

async function exists(filePath: string): Promise<boolean> {
  return stat(filePath).then(() => true, () => false)
}

function normalizeGameSettings(source: string): string {
  const skippedKeys = new Set(['entry', 'extlua_entry', 'extlua_preload'])
  return source
    .split(/\r?\n/)
    .filter((line) => {
      const match = line.match(/^\s*([a-z_][\w.]*)\s*:/i)
      return !match || !skippedKeys.has(match[1])
    })
    .join('\n')
    .trim()
}

async function loadGameSettings(id: string): Promise<string> {
  const filename = path.join(testDir, `${id}.game`)
  if (!(await exists(filename))) {
    return ''
  }
  return normalizeGameSettings(await readFile(filename, 'utf8'))
}

async function loadRuntimeFiles(root: string, prefix: string): Promise<ExampleRuntimeFile[]> {
  if (!(await exists(root))) {
    return []
  }

  const entries = await readdir(root, { withFileTypes: true })
  const files = await Promise.all(entries.map(async (entry) => {
    const fullPath = path.join(root, entry.name)
    const relativePath = prefix ? `${prefix}/${entry.name}` : entry.name
    if (entry.isDirectory()) {
      return loadRuntimeFiles(fullPath, relativePath)
    }
    if (!entry.isFile()) {
      return []
    }
    return [{
      path: relativePath,
      source: await readFile(fullPath, 'utf8'),
    }]
  }))

  return files.flat().sort((a, b) => a.path.localeCompare(b.path))
}

export async function loadExamples(): Promise<ExampleEntry[]> {
  const names = (await readdir(testDir))
    .filter(name => name.endsWith('.lua'))
    .sort()

  const examples = await Promise.all(
    names.map(async (name) => {
      const id = name.slice(0, -4)
      const source = await readFile(path.join(testDir, name), 'utf8')
      const gameSettings = await loadGameSettings(id)
      const runtimeFiles = await loadRuntimeFiles(path.join(testDir, id), id)
      return {
        id,
        title: titleize(id),
        entry: `test/${name}`,
        source,
        gameSettings,
        runtimeFiles,
      }
    }),
  )

  return examples
}

export async function loadDocs(): Promise<DocEntry[]> {
  const names = (await readdir(docsDir))
    .filter(name => name.endsWith('.lua'))
    .sort()

  const modules = await Promise.all(
    names.map(async (name) => {
      const moduleName = name.slice(0, -4)
      const fileContent = await readFile(path.join(docsDir, name), 'utf8')
      return {
        id: moduleName,
        module: moduleName,
        title: titleize(moduleName),
        blocks: parseDocFile(fileContent),
      }
    }),
  )

  return modules
}

function parseDocFile(content: string): DocBlock[] {
  const blocks: DocBlock[] = []
  let docLines: string[] = []
  let annos: string[] = []

  const flush = (signature: string | null) => {
    if (docLines.length === 0 && annos.every(anno => anno === 'meta' || anno.startsWith('meta '))) {
      docLines = []
      annos = []
      return
    }
    if (docLines.length === 0 && annos.length === 0) {
      return
    }
    blocks.push({
      signature: signature ?? annotationSignature(annos),
      docs: docLines,
      annos,
    })
    docLines = []
    annos = []
  }

  for (const line of content.split(/\r?\n/)) {
    if (line.startsWith('---@')) {
      annos.push(trim(line.replace(/^---@/, '')))
      continue
    }
    if (line.startsWith('---')) {
      docLines.push(trim(line.replace(/^---\s?/, '')))
      continue
    }

    const trimmed = trim(line)
    if (docLines.length > 0 || annos.length > 0) {
      if (trimmed === '') {
        flush(null)
        continue
      }
      flush(trimmed)
    }
  }

  flush(null)
  return blocks
}

function annotationSignature(annos: string[]): string | null {
  const anno = annos.find(anno =>
    anno.startsWith('alias ')
    || anno.startsWith('class ')
    || anno.startsWith('type ')
    || anno.startsWith('field '),
  )
  return anno ? `@${anno}` : null
}
