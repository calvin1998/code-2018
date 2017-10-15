load('Cell_values.mat')
Cell_values = Cell_values(:,:);


Battery_data = cell(12,1);
for Ci = 0:11
    current_index = Cell_values(:,1) == Ci;
    
    Battery_data{Ci+1,2} = Cell_values(current_index,:);
    
    text = sprintf('Cell %0.0f data',Ci+1);
    
    Battery_data{Ci+1,1} = text;
    
end



clearvars text Ci current_index

%subplot(3,1,1)
% for c = 0:11
%     plot(Battery_data{c+1,2}(:,2),'r','LineWidth',1)
% end

hold on 
cmap = hsv(12);
for c = 1:12
    plot(Battery_data{c,2}(:,2),'Color',cmap(c,:),'LineWidth',1)
end
hold off
title('Voltage vs. Time');
xlabel('Time (minutes)');
ylabel('Volts');
xlim([0,144]);
xticks(0:5:144);
yticks(0:0.05:10);
grid on

figure()
hold on 
cmap = hsv(12);
for c = 1:12
    plot(diff(Battery_data{c,2}(:,2)),'Color',cmap(c,:),'LineWidth',1)
end
hold off
title('Voltage vs. Time');
xlabel('Time (minutes)');
ylabel('Volts');
xlim([0,144]);
xticks(0:5:144);
yticks(0:0.05:10);
grid on

