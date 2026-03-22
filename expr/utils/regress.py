import numpy as np
import pandas as pd

csv_files = [
    "../result/b1b8/gap.csv",
    "../result/b1b8/google.csv",
    "../result/b1b8/ligra.csv",
    "../result/b1b8/ml.csv",
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
        row_2t8 = sub[sub["Prefetcher"] == "resize8to2"]
        if len(row_2way) == 0 or len(row_8way) == 0:
            continue

        row_2way = row_2way.iloc[0]
        row_8way = row_8way.iloc[0]
        row_2t8 = row_2t8.iloc[0]

        x = row_2t8["Resize_L3Hit_rate"]
        y = row_2t8["Resize_UPF_rate"]

        ipc2 = row_2way["IPCI"]
        ipc8 = row_8way["IPCI"]


        if max(ipc2,ipc8) == ipc2:
            label = True
        elif max(ipc2,ipc8) == ipc8:
            label = False
        if abs(ipc2-ipc8) < 0.03:
            continue

        dataset.append((t, x, y, label))

print("dataset size:", len(dataset))

for d in dataset[:10]:
    print(d)


uu = np.array([d[0] for d in dataset])
X = np.array([[d[1],d[2]] for d in dataset])
y = np.array([d[3] for d in dataset]).astype(float)
def compute_accuracy(w, X, y):
    print("w =", w)
    logits = X @ w 
    pred_label = logits > 0
    #print(uu[pred_label != y])
    return np.mean(pred_label == y)


best = 0
ii, jj= 0, 0
for i in range(-100,100):
    for j in range(-100,100):
    
        print(f"Testing w = [{i}, {j}]")
        res = compute_accuracy(np.array([i, j]), X, y)
        if res > best:
            best = res
            print(f"New best accuracy: {best:.4f} with w = [{i}, {j}]")
            ii, jj = i, j
w = np.array([ii,jj])
print(f"w = {ii},{jj}")


print("\nFinal accuracy: {:.4f}".format(compute_accuracy(w, X, y)))


# import matplotlib.pyplot as plt
# import numpy as np

# # print("\nFinal parameters:")

# print(f"w = {w}")


# X_zero = X[y == True]
# X_pos = X[y == False]

# # 画图
# plt.figure(figsize=(6, 6))

# plt.scatter(X_zero[:, 0], X_zero[:, 1], c='blue', label='0', s=15)
# plt.scatter(X_pos[:, 0], X_pos[:, 1], c='green', label='1', s=15)

# x_vals = np.linspace(X[:,0].min(), X[:,0].max(), 100)
# y_vals = 10 * x_vals / 11
# plt.plot(x_vals, y_vals, color='black', linewidth=2, label='x - y = 0')

# plt.xlabel('x')
# plt.ylabel('y')
# plt.title('Scatter Plot')
# plt.legend()
# plt.grid(True)

# plt.savefig("aa.png")



