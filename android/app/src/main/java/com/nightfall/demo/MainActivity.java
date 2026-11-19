package com.nightfall.demo;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;

// Offline browser-shell demo only. It contains no guest engine or game loader.
public final class MainActivity extends Activity {
    private static final int FILE_REQUEST = 11;
    private ValueCallback<Uri[]> pendingFile;
    private WebView browser;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        browser = new WebView(this);
        browser.getSettings().setJavaScriptEnabled(true);
        browser.getSettings().setDomStorageEnabled(true);
        browser.getSettings().setAllowFileAccess(true);
        browser.getSettings().setAllowFileAccessFromFileURLs(false);
        browser.getSettings().setAllowUniversalAccessFromFileURLs(false);
        browser.getSettings().setSupportMultipleWindows(false);
        browser.setWebViewClient(new WebViewClient() {
            @Override public boolean shouldOverrideUrlLoading(WebView view, String address) {
                Uri uri = Uri.parse(address);
                if ("file".equals(uri.getScheme()) && address.startsWith("file:///android_asset/")) return false;
                if ("https".equals(uri.getScheme())) {
                    try { startActivity(new Intent(Intent.ACTION_VIEW, uri)); }
                    catch (ActivityNotFoundException error) { Toast.makeText(MainActivity.this, "No browser found", Toast.LENGTH_SHORT).show(); }
                }
                return true;
            }
            @Override public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                return shouldOverrideUrlLoading(view, request.getUrl().toString());
            }
        });
        browser.setWebChromeClient(new WebChromeClient() {
            @Override public boolean onShowFileChooser(WebView view, ValueCallback<Uri[]> callback, FileChooserParams params) {
                if (pendingFile != null) pendingFile.onReceiveValue(null);
                pendingFile = callback;
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                intent.setType("*/*");
                try { startActivityForResult(intent, FILE_REQUEST); }
                catch (ActivityNotFoundException error) { pendingFile.onReceiveValue(null); pendingFile = null; return false; }
                return true;
            }
        });
        setContentView(browser);
        browser.loadUrl("file:///android_asset/index.html");
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != FILE_REQUEST || pendingFile == null) return;
        Uri[] files = result == RESULT_OK && data != null && data.getData() != null ? new Uri[]{data.getData()} : null;
        pendingFile.onReceiveValue(files); pendingFile = null;
    }
    @Override public void onBackPressed() {
        if (browser.canGoBack()) browser.goBack(); else super.onBackPressed();
    }
    @Override protected void onDestroy() {
        if (pendingFile != null) { pendingFile.onReceiveValue(null); pendingFile = null; }
        browser.destroy(); super.onDestroy();
    }
}
