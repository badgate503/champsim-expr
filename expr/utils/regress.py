import numpy as np
import pandas as pd

csv_files = [
    "../result/b1b8/gap.csv",
    "../result/b1b8/google.csv",
    "../result/b1b8/ligra.csv",
    "../result/b1b8/ml.csv",
    "../result/b1b8/spec06.csv",
    "../result/b1b8/spec17.csv"
]

dataset = []



# 

for f in csv_files:

    df = pd.read_csv(f,sep=",")

    traces = df["Trace"].unique()

    for t in traces:
        if(t=="Average"):
            continue
        sub = df[df["Trace"] == t]

        row_2way = sub[sub["Prefetcher"] == "baseline.2way"]
        row_8way = sub[sub["Prefetcher"] == "baseline.8way"]
        row_10w = sub[sub["Prefetcher"] == "resize10w"]
        if len(row_2way) == 0 or len(row_8way) == 0:
            continue

        row_2way = row_2way.iloc[0]
        row_8way = row_8way.iloc[0]
        row_10w = row_10w.iloc[0]

        x = row_10w["Resize_L3Hit_rate"] 
        y = row_10w["Resize_UPF_rate"]

        ipc2 = row_2way["IPCI"]
        ipc8 = row_8way["IPCI"]

        label = ipc8 < ipc2

        if abs(ipc8 - ipc2) < 0.03:
            continue

        dataset.append((t, x, y, label))

print("dataset size:", len(dataset))

for d in dataset[:10]:
    print(d)


uu = np.array([d[0] for d in dataset])
X = np.array([[d[1],d[2]] for d in dataset])
y = np.array([d[3] for d in dataset]).astype(float)



lr = 0.1
epochs = 100

def sigmoid(x):
    return 1 / (1 + np.exp(-x))
w = np.zeros(X.shape[1])
b = 0.0
for epoch in range(epochs):

    # 线性部分
    logits = X @ w + b

    # sigmoid
    preds = sigmoid(logits)

    # Binary Cross Entropy loss
    loss = -np.mean(
        y * np.log(preds + 1e-9) +
        (1 - y) * np.log(1 - preds + 1e-9)
    )

    # 梯度
    grad = preds - y

    dw = X.T @ grad / len(X)
    db = np.mean(grad)

    # 更新
    w -= lr * dw
    b -= lr * db

    if epoch % 100 == 0:
        print(f"epoch {epoch}, loss = {loss:.6f}")
def compute_accuracy(w, X, y):
    print("w =", w)
    logits = X @ w 
    pred_label = logits > 0
    #print(uu[pred_label != y])
    return np.mean(pred_label == y)
print("\nFinal parameters:")

print(f"w = {w}, b = {b}")

print("\nFinal accuracy: {:.4f}".format(compute_accuracy(w, X, y)))

# best = 0
# ii, jj, kk = 0, 0,0
# for i in range(-100,0):
#     for j in range(0,100):
#         for k in range(0,100):
#             print(f"Testing w = [{i}, {j}, {k}]")
#             res = compute_accuracy(np.array([i, j, k]), X, y)
#             if res > best:
#                 best = res
#                 print(f"New best accuracy: {best:.4f} with w = [{i}, {j}, {k}]")
#                 ii, jj, kk = i, j, k
# print(f"\nBest accuracy: {best:.4f} with w = [{ii}, {jj}, {kk}]")




