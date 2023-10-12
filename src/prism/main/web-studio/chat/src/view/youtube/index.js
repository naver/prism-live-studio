import Vue from 'vue'
import App from './Index.vue'
import '@/assets/css/reset.scss'

import { DEFAULT_LANG } from '@/assets/js/constants'
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
document.body.classList.add(lang)
Vue.prototype.$lang = lang
Vue.prototype.$i18n = i18n

Vue.config.productionTip = false

new Vue({
  render: h => h(App),
}).$mount('#app')
