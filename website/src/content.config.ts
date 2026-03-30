import { defineCollection, z } from 'astro:content'
import { loadDocs, loadExamples } from './lib/content'

const examples = defineCollection({
  loader: async () => loadExamples(),
  schema: z.object({
    title: z.string(),
    entry: z.string(),
    source: z.string(),
  }),
})

const docs = defineCollection({
  loader: async () => loadDocs(),
  schema: z.object({
    module: z.string(),
    title: z.string(),
    blocks: z.array(
      z.object({
        signature: z.string().nullable(),
        docs: z.array(z.string()),
        annos: z.array(z.string()),
      }),
    ),
  }),
})

export const collections = {
  docs,
  examples,
}
