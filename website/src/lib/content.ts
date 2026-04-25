import { readdir, readFile } from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

export interface ExampleEntry {
  id: string
  title: string
  entry: string
  source: string
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

export async function loadExamples(): Promise<ExampleEntry[]> {
  const names = (await readdir(testDir))
    .filter(name => name.endsWith('.lua'))
    .sort()

  const examples = await Promise.all(
    names.map(async (name) => {
      const id = name.slice(0, -4)
      const source = await readFile(path.join(testDir, name), 'utf8')
      return {
        id,
        title: titleize(id),
        entry: `test/${name}`,
        source,
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
