#!/usr/bin/env python3
"""TFLite 1D-CNN classifier training. Usage: python3 train_model.py --dataset data.csv --output model.tflite
Labels: 0=unknown 1=garage 2=lock 3=lora 4=anomaly 5=emergency. Target: <500KB for S3."""
import argparse,csv,sys,numpy as np
try:import tensorflow as tf
except:print("pip3 install tensorflow");sys.exit(1)

def build(n,c):
    return tf.keras.Sequential([
        tf.keras.layers.Input(shape=(n,1)),
        tf.keras.layers.Conv1D(16,7,activation='relu',padding='same'),
        tf.keras.layers.MaxPooling1D(2),tf.keras.layers.BatchNormalization(),
        tf.keras.layers.Conv1D(32,5,activation='relu',padding='same'),
        tf.keras.layers.MaxPooling1D(2),tf.keras.layers.BatchNormalization(),
        tf.keras.layers.Conv1D(64,3,activation='relu',padding='same'),
        tf.keras.layers.GlobalAveragePooling1D(),
        tf.keras.layers.Dense(32,activation='relu'),tf.keras.layers.Dropout(0.3),
        tf.keras.layers.Dense(c,activation='softmax')])

def to_tflite(model,out,X):
    conv=tf.lite.TFLiteConverter.from_keras_model(model)
    conv.optimizations=[tf.lite.Optimize.DEFAULT]
    def rep():
        for i in range(min(500,len(X))):yield[X[i:i+1].astype(np.float32)]
    conv.representative_dataset=rep
    conv.target_spec.supported_ops=[tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    conv.inference_input_type=conv.inference_output_type=tf.int8
    tfl=conv.convert();open(out,'wb').write(tfl)
    kb=len(tfl)//1024;print(f"Saved {out} ({kb}KB){'  OK' if kb<=500 else '  WARNING >500KB'}");return kb

p=argparse.ArgumentParser()
p.add_argument('--dataset',required=True);p.add_argument('--output',required=True)
p.add_argument('--epochs',type=int,default=30);p.add_argument('--input-len',type=int,default=256)
a=p.parse_args()
X,y=[],[]
with open(a.dataset) as f:
    next(csv.reader(f))
    for r in csv.reader(f):y.append(int(r[0]));X.append([float(v) for v in r[1:a.input_len+1]])
X=np.array(X,dtype=np.float32);y=np.array(y,dtype=np.int32)
nc=len(np.unique(y));X=((X-X.mean())/(X.std()+1e-8)).reshape(-1,a.input_len,1)
m=build(a.input_len,nc);m.compile(optimizer='adam',loss='categorical_crossentropy',metrics=['accuracy'])
m.fit(X,tf.keras.utils.to_categorical(y,nc),epochs=a.epochs,validation_split=0.2,batch_size=32)
to_tflite(m,a.output,X)
