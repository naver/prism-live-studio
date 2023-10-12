<template>
  <div :class="{'is-visi':isShow}" class="color-bar">
    <img :src="srcList[srcIndex]">
  </div>
</template>

<script>
import img from '@/assets/img/index.js'

export default {
  name: 'ColorBar',
  data () {
    return {
      srcList: img,
      srcIndex: 0,
      scrollTimer: 0,
      needReady: true,
      isShow: false
    }
  },
  methods: {
    showBar (callback = () => {}) {
      this.reset()
      this.isShow = true
      this.scrollTimer = setInterval(() => {
        console.log('readyready')
        if (this.srcIndex >= this.srcList.length) {
          callback()
          this.reset()
        } else {
          this.srcIndex++
        }
      }, 40)
    },
    hideBar () {
      this.isShow = false
      this.reset()
    },
    reset () {
      clearInterval(this.scrollTimer)
      this.srcIndex = 0
    }
  }
}
</script>

<style scoped>
.color-bar {
  position: absolute;
  z-index: 100;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  visibility: hidden;
}

.color-bar.is-visi {
  visibility: inherit;
}
</style>
