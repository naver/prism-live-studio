<template>
  <button class="prism-btn-close" :class="{'is-small': isSmall,'no-hover':!hasHover}" @click="$emit('click')"></button>
</template>

<script>
export default {
  name: 'prismBtnClose',
  props: {
    isSmall: {
      type: Boolean,
      default: false
    },
    hasHover: {
      type: Boolean,
      default: true
    }
  }
}
</script>

<style lang="scss">
  .prism-btn-close {
    width: 22px;
    height: 22px;
    background: url(~@/assets/img/btn-close-normal.svg) no-repeat;
    background-size: 100%;

    &.is-small {
      width: 16px;
      height: 16px;
    }

    &.no-hover:hover {
      background: url(~@/assets/img/btn-close-normal.svg);
      background-size: 100%;
    }

    &.no-hover:active {
      background: url(~@/assets/img/btn-close-normal.svg);
      background-size: 100%;
    }

    &:hover {
      background-image: url(~@/assets/img/btn-close-over.svg);
      background-size: 100%;
    }

    &:active {
      background-image: url(~@/assets/img/btn-close-click.svg);
      background-size: 100%;
    }
  }
</style>
