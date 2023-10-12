import Vue from 'vue'
import App from './App.vue'
import './assets/style/index.scss'
import './assets/fonts/fonts.scss'
import printLog from './assets/js/printLog'

Vue.config.productionTip = false
Vue.prototype.printLog = printLog
new Vue({
  render: h => h(App)
}).$mount('#app')
