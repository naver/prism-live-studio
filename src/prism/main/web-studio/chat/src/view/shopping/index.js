import Vue from 'vue'
import App from './Index.vue'
import '@/assets/css/reset.scss'

require('@/assets/js/lang').default(Vue)
require('../../components/components/plugins').default(Vue)

Vue.config.productionTip = false

new Vue({
  render: h => h(App),
}).$mount('#app')
