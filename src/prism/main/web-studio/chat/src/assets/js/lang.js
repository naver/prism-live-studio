
const DEFAULT_LANG = require('@/assets/js/constants').DEFAULT_LANG
let lang = DEFAULT_LANG
let i18n
try {
  let param = new URLSearchParams(document.location.search).get('lang')
  param = param.toLocaleLowerCase()
  i18n = require(`@/i18n/${param}.json`)
  lang = param
} catch (err) {
  i18n = require(`@/i18n/${lang}.json`)
}

export default function setLangs(Vue) {
  Vue.prototype.$lang = lang
  Vue.prototype.$i18n = i18n
}
