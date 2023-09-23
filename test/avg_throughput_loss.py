#encoding: utf-8
import matplotlib.pyplot as plt
import sys
import numpy as np

font_size = 15

paths = [   '/vagrant/tju_tcp/test/loss0/server.event.trace',
            '/vagrant/tju_tcp/test/loss1/server.event.trace',
            '/vagrant/tju_tcp/test/loss2/server.event.trace',
            '/vagrant/tju_tcp/test/loss3/server.event.trace',
            '/vagrant/tju_tcp/test/loss4/server.event.trace',
            '/vagrant/tju_tcp/test/loss5/server.event.trace',
            '/vagrant/tju_tcp/test/loss6/server.event.trace'
        ]

# 纵坐标
avg_thrps = []
# 横坐标
losses = ['0 %', '1 %', '2 %', '3 %', '4 %', '5 %', '6 %' ]


# 读取 DELV 事件
def read_trace(file):
    DELV_dic = {'utctime':[], 'seq':[], 'size':[], 'throughput':[]}
    start_time = 0
    with open(file, 'r', encoding='utf-8') as f:
        for num, line in enumerate(f):
            if(line=='\n'): continue # 跳过空行
            if('DELV' not in line): continue # 跳过非[DELV]事件行
            line = line.strip('\n')
            line = line.replace('[', '')
            line = line.replace(']', '')
            line_list = line.split(' ')
            info_list = line_list[2:]
            info_list = [item.split(':')[1] for item in info_list]
            DELV_dic['utctime'].append(int(line_list[0]))
            DELV_dic['seq'].append(int(info_list[0]))
            DELV_dic['size'].append(int(info_list[1])) 
            if start_time==0:
                start_time = int(line_list[0])
    DELV_dic['time'] = [item - start_time for item in DELV_dic['utctime']]
    DELV_dic['time'] = np.divide(DELV_dic['time'], 1000000)
    return DELV_dic

# 计算各窗口大小的平均吞吐率 (单位：Mbps)
def cal_avg_thrp(DELV_dic):
    time_start = DELV_dic['time'][0]
    time_end = DELV_dic['time'][-1]
    tot_thrp = 0
    for size in DELV_dic['size']:
        tot_thrp+=size
    avg_thrp = tot_thrp*8/(time_end-time_start)*0.000001
    return avg_thrp

# 绘制不同窗口折线图
def plot_avg_throughput(losses, avg_thrps):
    plt.plot(losses, avg_thrps, color='black')
    plt.scatter(losses, avg_thrps, s=25)
    plt.xlabel('loss', fontdict={'size':font_size})
    plt.ylabel('avg throughput (Mbps)', fontdict={'size':font_size})
    plt.title("rate:50Mbps delay:6ms", fontdict={'size':font_size-4})
    plt.ylim(ymin=0, ymax=max(avg_thrps)*1.05)
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig('/vagrant/tju_tcp/test/Avg_Throuput_loss.png', dpi=600)
    print("绘制成功, 图像位于/vagrant/tju_tcp/test/Avg_Throuput_loss.png")
    plt.cla()


for file in paths:
    DELV_dic = read_trace(file)
    avg_thrp = cal_avg_thrp(DELV_dic)
    avg_thrps.append(avg_thrp)

plot_avg_throughput(losses,avg_thrps)