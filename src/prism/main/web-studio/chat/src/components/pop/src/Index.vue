<template>
  <transition name="pop-fade">
    <div v-if="show" class="prism-pop">
      <div class="pop-box">
        <div class="pop-header">
          <span class="pop-header-title">{{title}}</span>
          <btn-close class="pop-btn-close" @click="handleCancel"></btn-close>
        </div>
        <div class="pop-content">
          <span v-html="content"></span>
        </div>
        <div class="pop-btns">
          <button class="pop-btn" ref="confirmBtn" @click="handleConfirm" @keydown.enter="handleKeydown">{{confirmText}}</button>
          <button class="pop-btn" v-if="type === 'confirm'" @click="handleCancel">{{cancelText}}</button>
        </div>
      </div> 
    </div>
  </transition>
</template>

<script>
import BtnClose from '@/components/BtnClose.vue'

export default {
  name: 'prismPop',
  components: {
    BtnClose
  },
  data() {
    return {
      activeElement: null,
      show: false,
      title: 'notice',
      content: '',
      cancelText: 'cancel',
      confirmText: 'confirm',
      type: 'confirm' /* alert, confirm */
    }
  },
  watch: {
    show(val) {
      if (val) {
        this.activeElement = document.activeElement
        document.body.classList.add('lock-scroll')
        this.$nextTick(() => {
          const btns = this.$el.getElementsByClassName('pop-btn')
          if (btns && btns.length) {
            const lastBtn = btns[btns.length - 1]
            lastBtn.focus()
          }
        })
      }
    }
  },
  methods: {
    handleConfirm() {
      this.handleClose()
    },
    handleCancel() {
      this.handleClose()
    },
    handleClose() {
      this.show = false
      if (this.activeElement) {
        this.activeElement.focus()
      }
      setTimeout(() => {
        document.body.classList.remove('lock-scroll')
      }, 200)
    },
    handleKeydown(e) {
      e.preventDefault()
      this.handleConfirm()
    }
  }
}
</script>

<style lang="scss">
  .lock-scroll {
    overflow: hidden;
  }
  
  .prism-pop {
    position: fixed;
    left: 0;
    right: 0;
    top: 0;
    bottom: 0;
    background: rgba(0, 0, 0, .5);
    z-index: 9999;

    .pop-box {
      position: absolute;
      display: inline-block;
      left: 0;
      right: 0;
      top: 50%;
      width: 90%;
      max-width: 410px;
      margin: 0 auto;
      transform: translateY(-50%);
      color: #ffffff;
      border: solid 1px #111111;
      background-color: #272727;
    }

    .pop-header {
      height: 41px;
      line-height: 40px;
      padding: 0 9px 0 17px;
      border-bottom: 1px solid #111111;
    }

    .pop-content {
      font-size: 14px;
      padding: 30px 25px;
      text-align: center;
    }

    .pop-btn-close {
      float: right;
      margin-top: 9px;
    }

    .pop-btns {
      font-size: 14px;
      padding: 0 20px 30px;
      text-align: center;

      .pop-btn {
        width: calc((100% - 10px) / 2);
        max-width: 140px;
        height: 40px;
        line-height: 40px;
        font-size: 14px;
        padding: 0 12px;
        background-color: #444444;
        border-radius: 3px;
        border: none;
        color: #fff;
        cursor: pointer;

        &:hover {
          background-color: #555555;
        }

        &:active {
          background-color: #2d2d2d;
        }

        & + button {
          margin-left: 10px;
        }
      }
    }
  }

  .pop-fade-enter-active .pop-box {
    animation: pop-fade-in .3s;
  }

  .pop-fade-leave-active .pop-box {
    animation: pop-fade-out .3s;
  }

  @keyframes pop-fade-in {
    0% {
      transform: translate3d(0, -20px, 0);
      opacity: 0;
    }
    100% {
      transform: translate3d(0, -50%, 0);
      opacity: 1;
    }
  }

  @keyframes pop-fade-out {
    0% {
      transform: translate3d(0, -50%, 0);
      opacity: 1;
    }
    100% {
      transform: translate3d(0, -20px, 0);
      opacity: 0;
    }
  }
</style>
