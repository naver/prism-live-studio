module.exports = {
  productionSourceMap: false,
  publicPath: './',
  outputDir: '../../data/chat',
  pages: {
    all: 'src/view/all/index.js',
    naver: 'src/view/naver/index.js',
    youtube: 'src/view/youtube/index.js',
    mqtt: 'src/view/mqtt/index.js',
    youtubev1: 'src/view/youtubev1/index.js',
    shopping: 'src/view/shopping/index.js'
  },
  devServer: {
    proxy: {
      '/api': {
        target: 'http://dev.api.prismlive.com/',
        changeOrigin: true,
        pathRewrite: {
          '^/api': ''
        }
      },
      '/shopping': {
        target: 'https://dev-global.apis.naver.com',
        changeOrigin: true,
        pathRewrite: {
          '^/shopping': ''
        }
      },
      '/globalV2': {
        target: 'http://dev-global.apis.naver.com',
        changeOrigin: true
      },
      '/prism': {
        target: 'http://dev-global.apis.naver.com',
        changeOrigin: true
      }
    }
  }
}
