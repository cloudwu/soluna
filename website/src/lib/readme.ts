import { readFile } from 'node:fs/promises'
import { marked } from 'marked'

const readmeUrl = new URL('../../../README.md', import.meta.url)
const repoUrl = 'https://github.com/cloudwu/soluna'

function normalizeGithubPath(pathname: string): string {
  if (pathname.startsWith('./')) {
    return pathname.slice(2)
  }
  if (pathname.startsWith('/../../')) {
    return pathname.slice('/../../'.length)
  }
  return pathname
}

function resolveReadmeHref(href: string, basePath: string): string {
  if (
    href.startsWith('http://')
    || href.startsWith('https://')
    || href.startsWith('#')
    || href.startsWith('mailto:')
  ) {
    return href
  }

  if (href === './docs') {
    return `${basePath}docs/`
  }
  if (href === './test') {
    return `${basePath}examples/`
  }

  const normalized = normalizeGithubPath(href)
  if (normalized === 'README.zh-CN.md') {
    return `${repoUrl}/blob/master/README.zh-CN.md`
  }
  if (normalized === 'LICENSE') {
    return `${repoUrl}/blob/master/LICENSE`
  }
  if (normalized.startsWith('.github/') || normalized.startsWith('docs/')) {
    return `${repoUrl}/tree/master/${normalized}`
  }
  if (normalized.startsWith('actions/') || normalized.startsWith('releases/')) {
    return `${repoUrl}/${normalized}`
  }

  return href
}

function rewriteReadmeLinks(markdown: string, basePath: string): string {
  return markdown.replace(/(!?\[[^\]]*\])\(([^)]+)\)/g, (_match, label, href) => {
    return `${label}(${resolveReadmeHref(href, basePath)})`
  })
}

export async function renderReadme(basePath: string): Promise<string> {
  const readme = await readFile(readmeUrl, 'utf8')
  const rewritten = rewriteReadmeLinks(readme, basePath)
  return marked.parse(rewritten) as string
}
