<script>
  import { onMount, createEventDispatcher } from 'svelte';
  const dispatch = createEventDispatcher();

  export let bssid;
  export let ssid;
  export let open;
  export let ap_mode;
  let loading = false;
  let password = "";
  let errorMessage = "";

  async function connect(){
    loading = true;
    errorMessage = "";
    let formData = new FormData();
    formData.append('bssid', bssid || "");
    formData.append('ssid', ssid || "");
    formData.append('password', password || "");
    formData.append('ap_mode', ap_mode || false);
    const res = await fetch(`/espconnect/connect`, { method: 'POST', body: new URLSearchParams(formData) });
		if (res.status === 200) {
      dispatch('success');
		} else {
      errorMessage = 'WiFi connection failed. Please verify your credentials.';
      try {
        const txt = await res.text();
        if (txt) {
          errorMessage = txt.trim().startsWith('{') ? (JSON.parse(txt).message || errorMessage) : txt;
        }
      } catch (e) { }
      // Keep the form visible: do not dispatch('error') here
    }
    loading = false;
		return res;
  }

  function back(){
    dispatch('back')
  }

  onMount(async () => {
    if(ap_mode){
      connect();
      setTimeout(() => window.location = "http://192.168.4.1/", 10000);
    }
  })
</script>

{#if ap_mode}
<div class="container d-flex flex-columns">
  <div class="row h-100">
    <div class="column">
      <div class="row">
        Activating AP mode...
      </div>
      <div class="row">
        You will be redirected to: <a href="http://192.168.4.1/">http://192.168.4.1/</a>
      </div>
    </div>
  </div>
</div>
{/if}

{#if !ap_mode}
<form class="container d-flex flex-columns" on:submit|preventDefault={connect}>
  <div class="row h-100">
    <div class="column">
      <div class="row">
        <div class="column text-center">
          <p class="mb-2 text-muted">
            Connect to WiFi
          </p>
        </div>
      </div>
      <div class="row">
        <div class="column column-100">
          <input type="text" placeholder="SSID" id="ssid" value={ssid} disabled={loading} autocomplete="off" required readonly>
        </div>
      </div>
      {#if !open}
      <div class="row">
        <div class="column column-100">
          <input type="password" placeholder="WiFi Password" id="password" bind:value={password} disabled={loading} autocomplete="off" required minlength="8">
        </div>
      </div>
      {/if}
      {#if errorMessage}
      <div class="row">
        <div class="column column-100">
          <div class="error-box">
            {errorMessage}
          </div>
        </div>
      </div>
      {/if}
    </div>
  </div>
  <div class="row flex-rows">
    <div class="column w-auto pr-1">
      <button type="button" class="button btn-light text-center" on:click={back} disabled={loading}>
        <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" style="fill: #fff; vertical-align: middle"><path d="M21 11L6.414 11 11.707 5.707 10.293 4.293 2.586 12 10.293 19.707 11.707 18.293 6.414 13 21 13z"></path></svg>
      </button>
    </div>
    <div class="column">
      <button type="submit" class="button w-100 text-center" disabled={loading}>
        {#if loading}
          <div class="btn-loader"></div>
        {:else}
          Connect
        {/if}
      </button>
    </div>
  </div>
</form>
{/if}

<style>
  .error-box{
    background: #fde9e9;
    color: #b00020;
    border: 1px solid #f5c2c2;
    padding: 0.75rem 1rem;
    border-radius: .5rem;
    margin-top: .5rem;
  }
</style>
