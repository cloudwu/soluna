import antfu from '@antfu/eslint-config'

export default antfu(
  {
    astro: true,
    typescript: true,
    stylistic: {
      semi: false,
    },
  },
  {
    rules: {
      'style/semi': ['error', 'never'],
    },
  },
)
