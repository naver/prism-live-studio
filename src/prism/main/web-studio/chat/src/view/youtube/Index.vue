<template>
  <div class="prism-chat-container">
    <div class="chart-info-container">
      <div class="tips-no-video">
        <i class="icon-chat"></i><br>
        <span v-html="text"></span>
      </div>
    </div>
    <form class="chart-send-container" @submit.prevent>
      <input
          class="input"
          type="text"
          :placeholder="$i18n.placeholder"
          disabled>
      <button class="icon-send" disabled></button>
    </form>
  </div>
</template>

<script>
import { handleJson } from '@/assets/js/utils'
import printLog from '@/assets/js/printLog'

export default {
  name: 'prismChatYoutbue',
  data () {
    return {
      text: this.$i18n.youtubeReady
    }
  },
  created () {
    window.addEventListener('prism_events', this.handlePrismEvents)
    window.sendToPrism(JSON.stringify({
      type: 'loaded',
      data: {
        platform: ['youtube']
      }
    }))
  },
  methods: {
    handlePrismEvents (event) {
      let detail = event.detail
      detail = handleJson(detail)
      let {type, data} = detail
      data = handleJson(data)
      switch (type) {
        case 'init':
          let r = data.platforms.find(item => item.name === 'YOUTUBE')
          r && r.isPrivate && (this.text = this.$i18n.noLiveYoutube)
          break
        case 'end':
        case 'platform_close':
          this.text = this.$i18n.youtubeReady
          break
      }
    }
  },
  beforeDestroy () {
    printLog({message: 'YOUTUBEChat: The chat page is closed'})
    window.removeEventListener('prism_events', this.handlePrismEvents)
  },
  mounted () {
    printLog({message: 'YOUTUBEChat: The chat page loads successfully'})
  }
}
</script>

<style lang="scss">
@import '~@/assets/css/chatsWrapper.scss';
</style>
