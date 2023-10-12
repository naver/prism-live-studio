import Pop from './src/index.js'

const install = function(Vue) {
  Vue.component(Pop.name, Pop)
  Vue.prototype.$alert = Pop.alert
  Vue.prototype.$confirm = Pop.confirm
}

export default install
