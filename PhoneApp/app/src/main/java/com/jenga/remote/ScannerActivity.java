package com.jenga.remote;

import android.Manifest;
import android.content.*;
import android.content.pm.PackageManager;
import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;
import androidx.camera.core.*;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.camera.view.PreviewView;
import androidx.core.content.ContextCompat;
import com.google.mlkit.vision.barcode.*;
import com.google.mlkit.vision.barcode.common.Barcode;
import com.google.mlkit.vision.common.InputImage;
import java.util.concurrent.*;

public final class ScannerActivity extends AppCompatActivity {
  private final ExecutorService analysis = Executors.newSingleThreadExecutor();
  private final BarcodeScanner scanner =
      BarcodeScanning.getClient(
          new BarcodeScannerOptions.Builder().setBarcodeFormats(Barcode.FORMAT_QR_CODE).build());
  private boolean found;

  @Override
  public void onCreate(Bundle state) {
    super.onCreate(state);
    setContentView(R.layout.activity_scanner);
    if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA)
        == PackageManager.PERMISSION_GRANTED) startCamera();
    else requestPermissions(new String[] {Manifest.permission.CAMERA}, 10);
  }

  @Override
  public void onRequestPermissionsResult(int code, String[] permissions, int[] results) {
    super.onRequestPermissionsResult(code, permissions, results);
    if (code == 10 && results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED)
      startCamera();
    else finish();
  }

  @androidx.camera.core.ExperimentalGetImage
  private void startCamera() {
    var future = ProcessCameraProvider.getInstance(this);
    future.addListener(
        () -> {
          try {
            ProcessCameraProvider provider = future.get();
            Preview preview = new Preview.Builder().build();
            preview.setSurfaceProvider(
                ((PreviewView) findViewById(R.id.preview)).getSurfaceProvider());
            ImageAnalysis images =
                new ImageAnalysis.Builder()
                    .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                    .build();
            images.setAnalyzer(
                analysis,
                proxy -> {
                  if (found || proxy.getImage() == null) {
                    proxy.close();
                    return;
                  }
                  scanner
                      .process(
                          InputImage.fromMediaImage(
                              proxy.getImage(), proxy.getImageInfo().getRotationDegrees()))
                      .addOnSuccessListener(
                          values -> {
                            for (Barcode barcode : values)
                              if (!found && barcode.getRawValue() != null) {
                                found = true;
                                setResult(
                                    RESULT_OK, new Intent().putExtra("qr", barcode.getRawValue()));
                                finish();
                                break;
                              }
                          })
                      .addOnCompleteListener(task -> proxy.close());
                });
            provider.bindToLifecycle(this, CameraSelector.DEFAULT_BACK_CAMERA, preview, images);
          } catch (Exception e) {
            android.widget.Toast.makeText(
                    this, "Camera could not start", android.widget.Toast.LENGTH_LONG)
                .show();
            finish();
          }
        },
        ContextCompat.getMainExecutor(this));
  }

  @Override
  public void onDestroy() {
    super.onDestroy();
    analysis.shutdown();
    scanner.close();
  }
}
