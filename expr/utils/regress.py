import numpy as np
import pandas as pd

csv_files = [
    "../result/gap.csv",
    "../result/google.csv",
    "../result/ligra.csv",
    "../result/ml.csv",
    "../result/spec17.csv"
]

dataset = []



# 

large = 0
small = 0

for f in csv_files:

    df = pd.read_csv(f,sep=",")

    traces = df["Trace"].unique()

    for t in traces:
        if(t=="Average"):
            continue
        sub = df[df["Trace"] == t]

        row_small = sub[sub["Prefetcher"] == "baseline.2way"]
        row_large = sub[sub["Prefetcher"] == "baseline.8way"]
        row_prism = sub[sub["Prefetcher"] == "prism_init2"]
        if len(row_small) == 0 or len(row_large) == 0:
            continue

        row_small = row_small.iloc[0]
        row_large = row_large.iloc[0]
        row_prism = row_prism.iloc[0]

        x = row_prism["Init_L3Hit_rate"]
        y = row_prism["Init_UPF_rate"]

        ipc2 = row_small["IPCI"]
        ipc8 = row_large["IPCI"]

        if abs(ipc2-ipc8) < 0.03:
            continue
        if max(ipc2,ipc8) == ipc2:
            label = True
            small+=1
        elif max(ipc2,ipc8) == ipc8:
            label = False
            large+=1
        

        dataset.append((t, x, y, label))
        if x>1 or y > 1:
            print(">1 "+t)
print("dataset size:", len(dataset))
print(large/(small+large))
# for d in dataset[:10]:
    # print(d)


uu = np.array([d[0] for d in dataset])
X = np.array([[d[1],d[2]] for d in dataset])
y = np.array([d[3] for d in dataset]).astype(float)
def compute_accuracy(w, X, y):
    
    logits = X @ w 
    pred_label = logits > 0
    #print(uu[pred_label != y])
    #print(f"w = {w}, precision = {np.mean(pred_label == y)}")
    return np.mean(pred_label == y)


best = 0
ii, jj= 0, 0
for i in range(-100,100):
    for j in range(-100,100):
    
        res = compute_accuracy(np.array([i, j]), X, y)
        if res > best:
            best = res
            print(f"New best accuracy: {best:.4f} with w = [{i}, {j}]")
            ii, jj = i, j
w = np.array([ii,jj])
print(f"w = {ii},{jj}")


print("\nFinal accuracy: {:.4f}".format(compute_accuracy(w, X, y)))


import matplotlib.pyplot as plt
import numpy as np

# print("\nFinal parameters:")

print(f"w = {w}")


X_zero = X[y == True]
X_pos = X[y == False]

# 画图
plt.figure(figsize=(6, 6))

plt.scatter(X_zero[:, 0], X_zero[:, 1], c='blue', label='0', s=15)
plt.scatter(X_pos[:, 0], X_pos[:, 1], c='green', label='1', s=15)

plt.xlabel('x')
plt.ylabel('y')
plt.title('Scatter Plot')
plt.legend()
plt.grid(True)

plt.savefig("aa.png")



