import axios from 'axios'

export default {
  delete(data) {
    const { accessToken, ...params } = data
    return axios.delete('https://www.googleapis.com/youtube/v3/liveChat/messages', {
      params,
      headers: {
        Authorization: `Bearer ${accessToken}`
      }
    })
  }
}
