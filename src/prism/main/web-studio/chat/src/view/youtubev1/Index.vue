<template>
  <div class="prism-chat-container">
    <div class="chart-info-container">
      <div class="tips-no-video">
        <img src="@/assets/img/destination-youtube.svg" alt="">
        <div class="youtube-v1-text">{{$i18n.youtubev1.text}}</div>
        <button class="youtubev1-button" :disabled="!url" @click="handleClick">{{$i18n.youtubev1.button}}</button>
      </div>
    </div>
  </div>
</template>

<script>
import { handleJson } from '@/assets/js/utils'
import printLog from '@/assets/js/printLog'

export default {
  name: 'prismChatYoutbue',
  data () {
    return {
      url: ''
    }
  },
  methods: {
    handleClick () {
      window.open(this.url, 'youtube-v1', 'width=600,height=800')
    }
  },
  created () {
    this.url = new URLSearchParams(window.location.search).get('chaturl')
  }
}
</script>

<style lang="scss">
  @import '~@/assets/css/chatsWrapper.scss';

  .youtubev1-button {
    width: 98px;
    height: 32px;
    border: 1px solid #444444;
    color: #999999;

    &:disabled, &:disabled:hover, &:disabled:hover:active {
      border-color: #2c2c2c;
      color: #555555;
      cursor: not-allowed;
    }

    &:hover {
      border: solid 1px #ffffff;
      color: #ffffff;
    }

    &:active {
      border: solid 1px #666;
      color: #666;
    }

  }

  .youtube-v1-text {
    margin-top: 10px;
    margin-bottom: 20px;
    white-space: break-spaces;
    color: #999999;
  }
</style>
