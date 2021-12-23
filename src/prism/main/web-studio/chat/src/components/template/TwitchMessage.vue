<template>
  <div class="message">
    <template v-for="(item, index) in message">
      <span v-if="item.type === MARK.TEXT"
        :key="index">{{item.data}}</span>
      <span v-else-if="item.type === MARK.AMOUNT"
        class="twitch-bits-amount"
        :style="{ color: item.color }"
        :key="index">{{item.data}}</span>
      <img v-else-if="item.type === MARK.EMOTICON"
        class="twitch-emoticon"
        :key="index"
        :src="handleTwitchEmoticonUrl(item.data)"
        alt="">
      <img v-else-if="item.type === MARK.BIT"
        class="twitch-bits-img"
        :key="index"
        :src="item.url"
        :alt="item.data">
    </template>
  </div>
</template>

<script>
import { TWITCH_EMOTICON_URL, TWITCH_BITS_LIBRARY } from '@/assets/js/twitchResources'

export default {
  name: 'template-twitch-chat',
  props: {
    data: {
      type: Object,
      default() {
        return {}
      }
    }
  },
  data() {
    return {
      bitsLibrary: TWITCH_BITS_LIBRARY,
      MARK: {
        TEXT: 0,
        EMOTICON: 1,
        AMOUNT: 2,
        BIT: 3
      }
    }
  },
  computed: {
    message() {
      let message = this.data.message

      // emoticion
      let emotesInfo = this.data.rawMessage.emotes
      let specialArr = []
      const MARK = this.MARK
      if (emotesInfo) {
        emotesInfo.split('/').forEach(el => {
          let item = el.match(/^(\d+):([\s\S]*)$/)
          if (item) {
            let posArr = item[2].split(',')
            posArr.forEach(pos => {
              let posArr = pos.match(/(\d+)-(\d+)/)
              if (posArr) {
                specialArr.push({
                  type: MARK.EMOTICON,
                  startIndex: Number(posArr[1]),
                  endIndex: Number(posArr[2]),
                  data: item[1]
                })
              }
            })
          }
        })
      }

      // bits
      if (this.data.rawMessage.bits && TWITCH_BITS_LIBRARY.bits) {
        let exec = TWITCH_BITS_LIBRARY.regexp.exec(message)
        while (exec) {
          // TODO
          const [ matchStr, bit, amount ] = exec
          const bitEndIndex = exec.index + bit.length - 1
          const amountLevel = TWITCH_BITS_LIBRARY.amounts.find((el, index, array) => el <= amount && (index === array.length - 1 || array[index + 1] > amount))

          specialArr.push({
            type: MARK.BIT,
            startIndex: exec.index,
            endIndex: bitEndIndex,
            data: bit,
            url: TWITCH_BITS_LIBRARY.bits[bit][amountLevel].image
          })
          specialArr.push({
            type: MARK.AMOUNT,
            startIndex: bitEndIndex + 1,
            endIndex: exec.index + matchStr.length - 1,
            data: amount,
            color: TWITCH_BITS_LIBRARY.bits[bit][amountLevel].color
          })
          exec = TWITCH_BITS_LIBRARY.regexp.exec(message)
        }
      }

      if (specialArr.length) {
        let msgArr = []
        let reIndex = 0
        specialArr.sort((a, b) => a.startIndex - b.startIndex).forEach((item, index, array) => {
          const { startIndex, endIndex, ...data } = item
          if (startIndex > reIndex) {
            msgArr.push({
              type: MARK.TEXT,
              data: message.slice(reIndex, startIndex)
            })
          }
          msgArr.push({...data})
          reIndex = endIndex + 1
          if (index === array.length - 1 && reIndex <= message.length - 1) {
            msgArr.push({
              type: MARK.TEXT,
              data: message.slice(reIndex)
            })
          }
        })
        message = msgArr
      } else {
        message = [{
          type: MARK.TEXT,
          data: message
        }]
      }
      return message
    }
  },
  methods: {
    handleTwitchEmoticonUrl(code) {
      return TWITCH_EMOTICON_URL.replace('{code}', code)
    }
  }
}
</script>

<style lang="scss">
  .chat-info-item {
    word-break: break-word;
    .twitch-emoticon {
      height: 28px;
      vertical-align: middle;
    }

    .twitch-bits-amount {
      font-weight: bold;
    }

    .twitch-bits-img {
      height: 20px;
      margin-right: 2px;
      vertical-align: middle;
    }
  }
</style>
