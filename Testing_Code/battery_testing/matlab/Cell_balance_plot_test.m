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

for c = 1:12
    subplot(4,4,c)
    hold on 
    plot(Battery_data{c,2}(:,2),'r','LineWidth',1)
    yyaxis right
    plot(Battery_data{c,2}(:,3),'b','LineWidth',1)

    hold off
    title('Voltage vs. Time');
    xlabel('Time (minutes)');
    ylabel('Volts');
    xlim([0,144]);
    xticks(0:10:144);
    yticks(0:0.25:10);
    grid on
end

