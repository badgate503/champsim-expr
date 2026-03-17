import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

# 示例数据（替换成你的）
x = [1,2,3,4,5,6,7]
y = [10,12,14,13,15,17,18]
z = [100,110,120,115,130,140,150]

fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')

ax.scatter(x, y, z)

ax.set_xlabel("Integer Variable")
ax.set_ylabel("Metric 1")
ax.set_zlabel("Metric 2")

plt.show()