// The Vue build version to load with the `import` command
// (runtime-only or standalone) has been set in webpack.base.conf with an alias.
import Vue from 'vue'
import App from './App'
import '@/style/index.css'
import ElementUI from 'element-ui'
import printLog from './printLog'

Vue.use(ElementUI)
Vue.config.productionTip = false
/* eslint-disable no-new */
Vue.prototype.$printLog = printLog
new Vue({
  el: '#app',
  data () {
    return {
      isTestAnimate: true
    }
  },
  components: {App},
  template: '<App/>'
})
