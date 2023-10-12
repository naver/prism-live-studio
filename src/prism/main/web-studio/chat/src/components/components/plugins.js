/**
 * virtual list plugin
 * https://github.com/Akryum/vue-virtual-scroller
 */
import 'vue-virtual-scroller/dist/vue-virtual-scroller.css'
import { DynamicScroller, DynamicScrollerItem } from 'vue-virtual-scroller'

import Pop from '@/components/pop/index'

export default function(Vue) {
  Vue.component('DynamicScroller', DynamicScroller)
  Vue.component('DynamicScrollerItem', DynamicScrollerItem)
  Vue.use(Pop)
}
