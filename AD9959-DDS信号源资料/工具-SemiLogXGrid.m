W = 251; % 宽度(像素)
H = 201; % 高度(像素)
XStart = 100E3; % 横轴起始值
XEnd = 100E6; % 横轴结束值
YGridN = 5; % 纵向网格数

G = zeros(H, W);
PointsPerDecade = (W - 1) / log10(XEnd / XStart);
XPos = 0;
PosFactor = log10(1:9);
n = 2;
while XPos(end) <= W
    XPos = [XPos, round(PosFactor(n) * PointsPerDecade)];
    n = n + 1;
    if n > 9
        PosFactor = PosFactor + 1;
        n = 1;
    end
end
XPos(end) = [];
YPos = 0 : round((H - 1) / YGridN) : (H - 1);

G(:, XPos+1) = ones(height(G), length(XPos));
G(YPos+1, :) = ones(length(YPos), width(G));

figure(Color='k');
imshow(G)

imwrite(G, 'SemiLogXGrid.bmp');
