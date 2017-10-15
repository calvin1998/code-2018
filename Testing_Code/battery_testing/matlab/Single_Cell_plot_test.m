load('Single_Cell_test.mat')
Single_Cell_Test = Single_Cell_Test(2:end,:);
[a,b] = size(Single_Cell_Test);
xStart = 0;
dx = 0.1;
N = a;
x = xStart + (0:N-1)*dx;
Single_Cell_Test(:,5) = x';

subplot(3,1,2)
plot(Single_Cell_Test(:,5)./60,Single_Cell_Test(:,2),'b','LineWidth',2)
title('Current vs. Time');
xlabel('Time (minutes)');
ylabel('Current (A)');
xlim([0,21]);
xticks(0:.5:100);
yticks(0:10:1000);
grid on
subplot(3,1,1)
plot(Single_Cell_Test(:,5)./60,Single_Cell_Test(:,3),'r','LineWidth',2)
title('Voltage vs. Time');
xlabel('Time (minutes)');
ylabel('Volts');
xlim([0,21]);
xticks(0:.5:100);
yticks(0:0.25:10);
grid on
subplot(3,1,3)
plot(Single_Cell_Test(:,5)./60,Single_Cell_Test(:,3).*Single_Cell_Test(:,2),'k','LineWidth',2)
title('Power vs. Time');
xlabel('Time (minutes)');
ylabel('Power (W)');
xlim([0,21]);
ylim([0,450]);
xticks(0:.5:100);
yticks(0:50:1000);
grid on